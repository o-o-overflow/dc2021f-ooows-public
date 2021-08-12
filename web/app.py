import eventlet
# Needed to get threading and stuff to work, so fun
eventlet.monkey_patch()

import argparse
import os
import secrets

import sqlite3

from flask import g
from flask import flash
from flask import Flask
from flask import render_template
from flask import request, redirect, url_for
from flask_socketio import SocketIO
from werkzeug.utils import secure_filename
from multiprocessing.managers import BaseManager

import vmm


env_or_default = lambda e, d: os.getenv(e) if os.getenv(e) else d

app = Flask(__name__)

app.secret_key = secrets.token_bytes(16)
#socketio = SocketIO(app, logger=True, engineio_logger=True)
socketio = SocketIO(app)

DISK_UPLOAD_DIR = env_or_default("OOOWS_DISK_UPLOAD_DIR", "/tmp/disks/")

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATABASE = os.path.join(BASE_DIR, 'ooows-web.db')

vmms = None

def make_dicts(cursor, row):
    return dict((cursor.description[idx][0], value)
                for idx, value in enumerate(row))

# initialize vmm workers if the db has VMs
def init_vmms():
    vmms = dict()
    vms = query_db("select * from vms")
    for vm in vms:
        vmms[vm['id']] = vmm.VmmWorker(name=vm['name'])
    return vmms

def get_db():
    db = getattr(g, '_database', None)
    if db is None:
        db = g._database = sqlite3.connect(DATABASE)
    db.row_factory = make_dicts
    return db

def get_vmm(vmid):
    global vmms
    if vmms is None:
        vmms = init_vmms()
    if not vmid in vmms:
        vmms[vmid] = vmm.VmmWorker()
    return vmms[vmid]

@app.teardown_appcontext
def close_connection(exception):
    db = getattr(g, '_database', None)
    if db is not None:
        db.close()

def query_db(query, args=(), one=False):
    cur = get_db().execute(query, args)
    rv = cur.fetchall()
    cur.close()
    return (rv[0] if rv else None) if one else rv

def init_db():
    with app.app_context():
        db = get_db()
        with app.open_resource('schema.sql', mode='r') as f:
            db.cursor().executescript(f.read())
        db.commit()

@app.route("/")
def index():
    # query db and list available VMs

    vms = query_db("select * from vms")
    for vm in vms:
        vm['running'] = not get_vmm(vm['id']).stopped()

    return render_template("index.html", vms=vms)

@app.route("/new", methods=['POST'])
def new():
    disk = request.files['disk']
    if not disk:
        flash("No file uploaded")
        return redirect(url_for('index'))

    vmname = disk.filename

    if not len(vmname) < 32:
        flash("VM name must be less than 32 character")
        return redirect(url_for('index'))

    if not vmname.isalnum():
        flash("VM name must be alphanumeric")
        return redirect(url_for('index'))

    VM_LIMIT = 3
    count = query_db("select count(id) as count from vms", one=True)
    if int(count['count']) >= VM_LIMIT:
        flash(f"Already have {VM_LIMIT} vms")
        return redirect(url_for('index'))

    vm = query_db("select id from vms where name = ?", [vmname], True)
    if not vm is None:
        # TODO: flash, vm with name already exists
        flash("VM with name already exists")
        return redirect(url_for('index'))

    # write this information to the sqlite database
    get_db().execute("insert into vms (name, disk) values(?, ?)",
                     (vmname, secure_filename(disk.filename)))

    get_db().commit()

    # the id of the vm will now be how we refer to it
    vm = query_db("select id, name from vms where name = ?", [vmname], True)
    if vm is None:
        # TODO : flash, weird error
        return redirect(url_for('index'))

    diskpath = os.path.join(DISK_UPLOAD_DIR, secure_filename(vmname))
    disk.save(diskpath)

    # instantiate runtime store using the vmm manager
    vmm = get_vmm(vm['id'])
    vmm.new(vm['name'], diskpath)

    return redirect(url_for('index'))

@app.route('/delete/<int:vmid>')
def delete(vmid):

    # get the vmname and disk file
    vm = query_db("select id, name, disk from vms where id = ?", [vmid], True)
    if vm is None:
        # TODO: flash 'invalid vm'
        flash("Invalid VM")
        return redirect(url_for('index'))

    vmm = get_vmm(vm['id'])
    if not vmm.stopped():
        vmm.stop()

    vmm.delete()

    global vmms
    del vmms[vm['id']]

    # delete the row from the table
    get_db().execute("delete from vms where id = ?", [vmid])
    get_db().commit()

    return redirect(url_for('index'))

@app.route('/start/<int:vmid>')
def start(vmid):
    # resolve vm id to get disk and name info

    vm = query_db("select id, name, disk from vms where id = ?",
                  [vmid], True)
    if vm is None:
        # TODO: flash 'invalid vm'
        flash("Invalid VM")
        return redirect(url_for('index'))

    # start the thread running the vmm
    vmm = get_vmm(vm['id'])

    # make sure VM is not already running (in db)
    if not vmm.stopped():
        # TODO: flash 'vm already running'
        flash("VM already running")
        return redirect(url_for('index'))

    vmm.start()

    return redirect(url_for('index'))

@app.route('/stop/<int:vmid>')
def stop(vmid):
    # resolve vm id to get disk and name info

    vm = query_db("select id, name, disk from vms where id = ?",
                  [vmid], True)
    if vm is None:
        # TODO: flash 'invalid vm'
        flash("Invalid VM")
        return redirect(url_for('index'))

    vmm = get_vmm(vm['id'])

    # make sure VM is already running
    if vmm.stopped():
        # TODO: flash 'vm not running'
        flash("VM not running")
        return redirect(url_for('index'))

    vmm.stop()

    return redirect(url_for('index'))

@app.route('/console/<int:vmid>')
def console(vmid):

    # resolve vm id
    vm = query_db("select id, name from vms where id = ?", [vmid], True)
    if vm is None:
        # TODO: flash 'invalid vm'
        flash("Invalid VM")
        return redirect(url_for('index'))

    # get vmm session
    vmm = get_vmm(vm['id'])

    # make sure vm is already running
    if vmm.stopped():
        # TODO: flash 'vm not running'
        flash("VM not running")
        return redirect(url_for('index'))

    # drop client into template with websocket console session
    return render_template("console.html", vm=vm)

@app.route('/view/<int:vmid>')
def view(vmid):

    # resolve vm id to get virtd session id
    vm = query_db("select id, name from vms where id = ?", [vmid], True)
    if vm is None:
        # TODO: flash 'invalid vm'
        flash("Invalid VM")
        return redirect(url_for('index'))

    vmm = get_vmm(vm['id'])

    # make sure vm is already running
    if vmm.stopped():
        # TODO: flash 'vm not running'
        flash("VM not running")
        return redirect(url_for('index'))

    return render_template("view.html", vm=vm)

@socketio.event
def view_video(message):
    # User is ready for a websocket VGA connection, so give it to them
    vmid = int(message['vmid'])

    vmm = get_vmm(vmid)
    vmm.video(socketio, vmid)
    return

@socketio.event
def view_console(message):
    # User is ready for a websocket console connection, so give it to them
    vmid = int(message['vmid'])

    vmm = get_vmm(vmid)
    vmm.console(socketio, vmid)
    return

@socketio.event
def console_rx(message):
    # User is ready for a websocket console connection, so give it to them
    vmid = int(message['vmid'])

    vmm = get_vmm(vmid)
    vmm.txConsoleData(message['data'])
    return


@socketio.event
def connect():
    return


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="app")
    parser.add_argument("--debug", action="store_true", help="Enable debugging")
    parser.add_argument("--port", type=int, default=5000, help="Port to listen on [default: 5000]")
    parser.add_argument("--host", default='127.0.0.1', help="Host to listen on [default: 127.0.0.1]")

    args = parser.parse_args()

    socketio.run(app, host=args.host, port=args.port, debug=args.debug)

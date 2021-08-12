#!/usr/bin/env python3
import argparse
import pathlib

import app

def main(tmp_dir):
    pathlib.Path(tmp_dir).mkdir(parents=True, exist_ok=True)
    app.init_db()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="init-db")
    parser.add_argument("--tmp-dir", default="/tmp/disks/", help="pcap file to use")

    args = parser.parse_args()

    main(args.tmp_dir)

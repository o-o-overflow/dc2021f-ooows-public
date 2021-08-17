## P9FS

This challenge was the first virtio device in the OOOWS family of challenges. It presented a simple implementation of the 9P protocol exposing a host-side file share allowing the guest and the host to share files. It largely followed the spec laid out here:

This challenge was intended to be the easiest of the devices and to encourage players to introduce features into their kernels such as virt queue and SMP support.

### Bugs

* Double fetch in Tmessage parsing.
As soon as a message is picked up in the request queue, the message body is parsed to transform the message into a TMessage object. The 9P protocol includes a message length in all message types used to know the bounds of the incoming message for parsing. The contents of the message at this point are in a buffer in guest memory. To process the request further the handler attempts to copy the message into a new buffer allocated based on the size field of the incoming message. Since the size field exists in guest memory another vCPU can change the value of the size field while the device is processing it. This leads to a buffer overflow in the heap of the device, which can be exploited by having another vCPU change the size field of the outgoing message to first allocate a small message (by keeping the value of the size field small) then perform a larger copy (by changing the value of the field).

* Use-after-free in TWalk
The 9P TWalk message type performs a directory walk to from an origin fid and places the result in a user-supplied new fid argument. The TWalk message itself supports some unique semantics, such as the ability to clone the origin fid into the new fid if no path to walk is provided. The origin fid and the new fid can also be identical, causing TWalk to place the result of the directory walk into origin fid, closing the resource pointed to by origin fid. A shallow UAF exists in the functionality if you request a TWalk where the origin fid and the new fid are identical and no path is provided. This causes the origin fid to close and free the underlying resource object without replacing the fid to resource mapping. This results in a dangling pointer access when subsequently referring to the origin fid.
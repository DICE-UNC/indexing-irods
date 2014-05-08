#!/usr/bin/env python
import sys
from proton import *

address = sys.argv[1]
body = sys.argv[2]

mng = Messenger()
mng.start()

msg = Message()
msg.address = address
msg.body = body
mng.put(msg)

mng.send()

mng.stop()

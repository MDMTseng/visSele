import hashlib, base64

h = hashlib.sha1("b7G7vibt50OJ4WmRE80B6Q==258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
print "hexdigest:", h.hexdigest() # hexadecimal string representation of the digest
print "digest:", h.digest() # raw binary digest
print
print "wrong result:", base64.b64encode(h.hexdigest())
print "right result:", base64.b64encode(h.digest())

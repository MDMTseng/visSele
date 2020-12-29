import usb.core


#/usr/sbin/diskutil activity


#install pyusb
#install libusb1.0.X
dev = usb.core.find(find_all=1)
for cfg in dev:
	print (cfg.product)
	print ('Decimal VendorID=' + str(hex(cfg.idVendor)) + ' & ProductID=' + str(cfg.idProduct) + '\n')
	print ('Hexadecimal VendorID=' + hex(cfg.idVendor) + ' & ProductID=' + hex(cfg.idProduct) + '\n\n')
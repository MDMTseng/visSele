import sys
import glob
import serial
from time import sleep
import minimalmodbus
import math

# port = '/dev/tty.usbserial-AQ00BYCR'
# baudrate = 38400
# client = ModbusClient(
#   method = 'rtu'
#   ,port='/dev/tty.usbserial-AQ00BYCR'
#   ,baudrate=baudrate
#   ,parity = 'O'
#   ,timeout=1
#   )





def serial_ports():
  """ Lists serial port names

    :raises EnvironmentError:
      On unsupported or unknown platforms
    :returns:
      A list of the serial ports available on the system
  """
  if sys.platform.startswith('win'):
    ports = ['COM%s' % (i + 1) for i in range(256)]
  elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
    # this excludes your current terminal"/dev/tty"
    ports = glob.glob('/dev/tty[A-Za-z]*')
  elif sys.platform.startswith('darwin'):
    ports = glob.glob('/dev/tty.*')
  else:
    raise EnvironmentError('Unsupported platform')
  return ports
  result = []
  for port in ports:
    try:
      print("try port:",port)
      s = serial.Serial(port)
      s.close()
      result.append(port)
    except (OSError, serial.SerialException):
      pass
  return result
 

def sdsd(port):
  ser = serial.Serial(
    port=port,
    baudrate=115200,
    parity=serial.PARITY_ODD,
    stopbits=serial.STOPBITS_TWO,
    bytesize=serial.SEVENBITS
  )


  print("ser.isOpen():",ser.isOpen())
  command = b'\x48\x65\x6c\x6c\x6f'
  ser.write(command)

# print(serial_ports())
port='/dev/tty.usbserial-DA01QYAR'
# port='/dev/tty.SLAB_USBtoUART'
DEVICE1 = minimalmodbus.Instrument(port, 0)  # port name, slave address (in decimal)
DEVICE1.serial.baudrate = 115200
# DEVICE1.address=0;
# DEVICE1.write_register(registeraddress=978,value=1,number_of_decimals=0,functioncode=6)  #Set modbus id
# DEVICE1.write_register(registeraddress=998,value=1,number_of_decimals=0,functioncode=6)  #save into EEPROM
DEVICE1.address=1;
# DEVICE1.write_register(registeraddress=978,value=1,number_of_decimals=0,functioncode=6)  # Registernumber, number of decimals
# DEVICE1.read_register(registeraddress=978,value=1,number_of_decimals=0,functioncode=6)  # Registernumber, number of decimals



def RunGo(MODEBUS_DEV,GX=1,SPEED=600,X=0,Y=0):

  MODEBUS_DEV.write_register(registeraddress=907,value=GX,functioncode=6)# GX
  MODEBUS_DEV.write_register(registeraddress=908,value=SPEED,functioncode=6)#Speed mm/min
  byteorder= minimalmodbus.BYTEORDER_BIG
  MODEBUS_DEV.write_long(registeraddress=1014,value=       X, signed=True, byteorder=  byteorder)#0.001mm
  MODEBUS_DEV.write_long(registeraddress=2014,value=       Y, signed=True, byteorder=  byteorder)#0.001mm
  MODEBUS_DEV.write_long(registeraddress=3014,value=  0*1000, signed=True, byteorder=  byteorder)#0.001mm
  MODEBUS_DEV.write_long(registeraddress=4014,value=  0*1000, signed=True, byteorder=  byteorder)#0.001mm


  # print("speed:",DEVICE1.read_register(registeraddress=908),"mm/min")
  # print("Axis1:",DEVICE1.read_long(registeraddress=1014,signed=True, byteorder=  byteorder),"0.001mm")
  # print("Axis2:",DEVICE1.read_long(registeraddress=2014,signed=True, byteorder=  byteorder),"0.001mm")
  MODEBUS_DEV.write_register(registeraddress=960,value=2,functioncode=6)#2 means run



# RunGo(DEVICE1,X=20*1000,Y=0)
loopN = 5
i=0
for i in range(loopN):
  turns=1
  R=30
  X=int(( R*math.sin(i*turns*2*3.1415/loopN)     )*1000)
  Y=int(( R*math.cos(i*turns*2*3.1415/loopN) +200)*1000)
  # print("X:",X," Y:",Y);

  RunGo(DEVICE1,GX=1,X=X,Y=Y,SPEED=6000)
  sleep(0.1)




while True:
  sleep(1)
  
  print(bin(DEVICE1.read_register(registeraddress=9000,functioncode=4)))





if __name__ == '__main__':
  # print(serial_ports())
  None
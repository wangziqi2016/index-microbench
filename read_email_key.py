#
# read_email_key.py - This file processes the email insert and workload file
#                     and outputs a file to use the index
#
#
# Run this file with the following arguments:
#
# python read_email_key.py [load file] [workload file] [27MB load file]
#

import sys
import os

def read_load_file():
  """
  This function reads the load file and returns a dict that maps a string
  to its line number (i.e. index)
  """
  filename = sys.argv[1]
  if os.path.isfile(filename) is False:
    raise TypeError("Illegal load file: %s" % (filename, ))
  
  fp = open(filename, "r")
  line_num = 0
  for line in fp:
    line = line.strip()
    index = line.find(" ")
    if index == -1 or \
       line[:index] != "INSERT":
      raise ValueError("Illegal line @ %d" % (line_num, ))

    ret[line[index + 1:]] = line_num

    print line[index + 1:]

    line_num += 1

  fp.close()
  return ret

  

if len(sys.argv) != 4:
  print("This program must take three arguments!")
  print("python read_email_key.py [load file] [workload file] [27MB load file]")
  sys.exit(1)



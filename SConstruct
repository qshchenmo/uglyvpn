import os

env = Environment()

debug = ARGUMENTS.get('debug', 0)

#if int(debug):
env.Append(CCFLAGS = '-g')

Export('env')

objs = []
cwd  = os.getcwd()
list = os.listdir(cwd)

uvpndir = ["uvpnd", "util" , "crypto"]

for item in uvpndir:
    if os.path.isfile(os.path.join(cwd, item, 'SConscript')):
        objs = objs + SConscript(os.path.join(item, 'SConscript'))

env.Program('uvpn',objs, LIBS = ['librt', 'libssl', 'crypto','cryptoc'], LIBPATH = 'crypto')


uvpnctldir = ["uvpnctl", "util"]
objs = []
for item in uvpnctldir:
    if os.path.isfile(os.path.join(cwd, item, 'SConscript')):
        objs = objs + SConscript(os.path.join(item, 'SConscript'))

env.Program('uctl',objs)
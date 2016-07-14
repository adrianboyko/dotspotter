from distutils.core import setup, Extension

setup(name='imgsum', version='1.0', \
      ext_modules=[Extension('imgsum',['imgsum.c'])])


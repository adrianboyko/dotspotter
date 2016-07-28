from distutils.core import setup, Extension

setup(
	name='imgsum', 
	version='1.1',
	ext_modules=[
		Extension(
			'imgsum',
			['imgsum.c'],
			extra_compile_args=["-std=c11"],
		)
	]
)


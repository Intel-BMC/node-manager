from setuptools import setup
import glob

setup(name='overlay-generator',
      version='0.1',
	  packages=['utils'],
      scripts=['overlay_gen.py'],
      data_files=[('overlay_templates', glob.glob('overlay_templates/*'))]
      )

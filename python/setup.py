#!/usr/bin/env python
'''
Wraprun package setup script.
'''

from setuptools import setup, find_packages

if __name__ == '__main__':

    setup(name='Wraprun',
          version='0.2.1',
          description='Flexible Aprun task bundler',
          author='Matt Belhorn',
          author_email='belhornmp@ornl.gov',
          url='https://github.com/olcf/wraprun',
          license="MIT",
          packages=find_packages(),
          install_requires=['pyyaml>=3.1.0'],
          scripts=['bin/wraprun'])

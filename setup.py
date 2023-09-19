from setuptools import setup, Extension, find_packages
import subprocess

from Cython.Build import cythonize
import Cython.Distutils

# Run the script
subprocess.run(['sh', 'build_ddup.sh'])

# Set the compilers
modules = cythonize(
           [
             Extension('miniprof.sampler',
                    sources = [
                        'miniprof/sampler.c',
                    ],
                    include_dirs = [
                        'vendored/dd-trace-py/ddtrace/internal/datadog/profiling/include',
                    ],
                    extra_objects = [
                        'libuploader.a',
                        'vendored/dd-trace-py/ddtrace/internal/datadog/libdatadog/lib/libdatadog_profiling.a',
                    ],
                    libraries=['stdc++'],
                    extra_compile_args=['-std=c11', '-O3']
             ),
             Cython.Distutils.Extension(
               "miniprof.profiler",
               sources=["miniprof/_profiler.pyx"],
               language="c",
             ),
           ]
         )
setup(name = 'miniprof',
      version = '0.3.2',
      description = 'A very small profiler',
      ext_modules = modules,
      entry_points={
          "console_scripts": [
              "miniprof-run= miniprof.profiler:main",
          ],
      },
    setup_requires=["setuptools_scm[toml]>=4", "cython", "cmake>=3.24.2; python_version>='3.6'"],
    packages=find_packages(),
)

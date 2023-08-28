from setuptools import setup, Extension, find_packages
import subprocess

# Run the script
subprocess.run(['sh', 'build_ddup.sh'])

# Set the compilers
module = Extension('miniprof.sampler',
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
                )
setup(name = 'miniprof',
      version = '0.2',
      description = 'Profiling I guess',
      ext_modules = [module],
      entry_points={
          "console_scripts": [
              "miniprof-run= miniprof.profiler:main",
          ],
      },
    packages=find_packages(),
)

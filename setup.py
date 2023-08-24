from setuptools import setup, Extension, find_packages

# Set the compilers
module = Extension('miniprof.sampler',
                    sources = [
                        'miniprof/sampler.c',
                    ],
                    extra_compile_args=['-O3']
                )
setup(name = 'miniprof',
      version = '0.1',
      description = 'Profiling I guess',
      ext_modules = [module],
      entry_points={
          "console_scripts": [
              "miniprof-exec= miniprof.profiler:main",
          ],
      },
    packages=find_packages(),
)

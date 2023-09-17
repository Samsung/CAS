import sys
import os

project = 'CAS'
copyright = '2022, a.niec@samsung.com'
author = 'a.niec@samsung.com'
release = '1.0'

extensions = ['sphinx.ext.autodoc']

templates_path = ['_templates']
exclude_patterns = ['test_main.py']

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

sys.path.insert(0, os.path.abspath('../..'))
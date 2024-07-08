import os
import re
import pytest
import json
import fnmatch

from libft_db import FTDatabase
from libcas import CASDatabase
from typing import List, Optional, Generator

from client.filtering import FtdbSimpleFilter, FilterException
from client.argparser import get_args, merge_args
from client.cmdline import process_commandline


ftdb = FTDatabase()
ftdb.load_db(os.path.join(os.environ.get('FTDB_DIR','.'), os.environ.get('FTDB', 'vmlinux_db.img')), quiet=True, debug=False)
cas_db = CASDatabase()

def parse_json(res:str, cmd:str) -> None:
    """
        Function to check whether provided string (mainly command result) is in json format 
    """
    try:
        json.loads(res)
        assert True
    except json.JSONDecodeError:
        assert False, f'Failed to parse {cmd} results'

@pytest.mark.parametrize(
    'cmd', [
            'ftdb-version',
            'ftdb-dir',
            'ftdb-module',
            'ftdb-release',
            'sources',
            'srcs',
            'modules',
            'mds',
            'funcs',
            'functions',
            'globs',
            'globals',
            'funcdecls'
            ]
)
def test_json(cmd):
    res = process_commandline(cas_db, cmd + ' --json', ftdb)
    parse_json(res, cmd + ' --json')

def get_ftdb_filter(args:List[str], ftdb:FTDatabase) -> Optional[FtdbSimpleFilter]:
    try:
        common_args, pipeline_args, _ = get_args(args)
        parsed_args = merge_args(common_args, pipeline_args[0])
        return FtdbSimpleFilter(parsed_args.ftdb_simple_filter, None, None, None, ft_db=ftdb)
    except FilterException as err:
        assert False, str(err)


class TestMisc:
    pass

class TestFtdbFilter:

    @pytest.mark.parametrize(
        'cmd,keywords',
        (
            ('sources',['path', 'has_func', 'has_global', 'has_funcdecl', 'name']),
            ('funcs', ['location', 'name'])
        )
    )
    def test_parse_options_src(self, cmd, keywords):
        for keyword in keywords:
            fil = get_ftdb_filter([cmd, f'--ftdb-filter=[{keyword}=*dfd,type=wc]'], ftdb)
            assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 1
            assert fil.filter_dict[0][0][keyword] == "*dfd" and keyword+"_pattern" in fil.filter_dict[0][0]
    
    def test_parse_anded(self):
        fil:FtdbSimpleFilter = get_ftdb_filter(['sources', '--ftdb-filter=[path=/abc,type=sp]and[has_func=*alloc*,type=wc]'], ftdb)
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 2
        assert fil.filter_dict[0][0]["path"] == "*/abc*"
        assert fil.filter_dict[0][1]["has_func"] == "*alloc*"

    def test_parse_ored(self):
        fil:FtdbSimpleFilter = get_ftdb_filter(['sources', '--ftdb-filter=[path=/abc,type=sp]or[has_func=*alloc*,type=wc]'], ftdb)
        assert len(fil.filter_dict) == 2 and len(fil.filter_dict[0]) == 1
        assert fil.filter_dict[0][0]["path"] == "*/abc*"
        assert fil.filter_dict[1][0]["has_func"] == "*alloc*"


class TestFtdbInfo:

    def test_directory(self):
        assert type(ftdb.get_dir()) == str
    
    def test_directory_cmd(self):
        res = process_commandline(cas_db, 'ftdb-dir', ftdb)
        assert type(res) == str
    
    def test_version(self):
        res = ftdb.get_version()
        assert type(res) == str
        assert re.match(r'\d\.\d\d', res) is not None
    
    def test_version_cmd(self):
        res = process_commandline(cas_db, 'ftdb-version', ftdb)
        assert type(res) == str
        assert re.match(r'\d\.\d\d', res) is not None

    def test_module(self):
        assert type(ftdb.get_module_name()) == str
    
    def test_md_cmd(self):
        res = process_commandline(cas_db, 'ftdb-module', ftdb)
        assert type(res) == str
    
    def test_release(self):
        assert type(ftdb.get_release()) == str
    

class TestSources:

    base_cmd = 'sources'
    
    @pytest.fixture
    def src_count(self):
        return int(process_commandline(cas_db, self.base_cmd + ' --count', ftdb))

    @pytest.mark.parametrize(
        'filter_cmd,expected_res,pathlike,path_pattern', [
            ('', (1000, 5000), False, None),
            (' --ftdb-filter=[path=*/soc/*,type=wc]', (100, 500), True, '*/soc/*'),
            (' --ftdb-filter=[has_global=*kvm*,type=wc]', (10, 50), False, None),
            (' --ftdb-filter=[has_func=*cpy,type=wc]', (1000, 2000), False, None),
            (' --ftdb-filter=[has_funcdecl=*cpy,type=wc]', (1, 10), False, None),
            (' --ftdb-filter=[has_func=*alloc*,type=wc]and[has_func=memcpy,type=sp]', (1000, 4000), False, None),
            (' --ftdb-filter=[has_global=*kvm*,type=wc]and[has_global=sleep,type=sp]', (0, 10), False, None),
            (' --ftdb-filter=[has_funcdecl=*alloc*,type=wc]or[has_funcdecl=memcpy,type=sp]', (40, 60), False, None),
        ]
    )
    def test_filter_options(self, filter_cmd, expected_res, pathlike, path_pattern):
        res_count = process_commandline(cas_db, self.base_cmd + filter_cmd + ' --count', ftdb)
        assert expected_res[0] <= int(res_count) < expected_res[1]

        if pathlike:
            res = process_commandline(cas_db, self.base_cmd + filter_cmd, ftdb)
            for path in res:
                assert fnmatch.fnmatch(path, path_pattern)

    @pytest.mark.parametrize(
        'keyword', [
            'name',
            'location'
        ]
    )
    def test_filter_unavailable(self, keyword):

        with pytest.raises(FilterException) as err:
            process_commandline(cas_db, self.base_cmd + f" --ftdb-filter=[{keyword}=*cpy,type=wc]", ftdb)

        assert f'Filter option "{keyword}" can be used' in str(err)

    @pytest.mark.parametrize(
        'filter_cmd', [
            (' --ftdb-filter=[path=*/soc/*,type=wc]'),
            (' --ftdb-filter=[has_global=*kvm*,type=wc]'),
            (' --ftdb-filter=[has_func=*cpy,type=wc]'),
            (' --ftdb-filter=[has_funcdecl=*cpy,type=wc]'),
            (' --ftdb-filter=[has_func=*alloc*,type=wc]and[has_func=memcpy,type=sp]'),
            (' --ftdb-filter=[has_global=*kvm*,type=wc]and[has_global=sleep,type=sp]'),
            (' --ftdb-filter=[has_funcdecl=*alloc*,type=wc]or[has_funcdecl=memcpy,type=sp]'),
        ]
    )
    def test_no_errs(self, filter_cmd):
        res = process_commandline(cas_db, self.base_cmd + filter_cmd, ftdb)

        assert 'Error' not in res
    
    @pytest.mark.parametrize(
        'pipe_cmd,expected_count', [
            ('globs ',(3000, 5000)),
            ('funcs ', (3000, 5000)),
            ('funcdecls ', (100, 500)),
            ('globs --ftdb-filter=[name=*kvm*,type=wc] ',(10, 50)),
            ('funcs --ftdb-filter=[name=*_desc,type=wc] ', (1000, 3000)),
            ('funcdecls --ftdb-filter=[name=*cpy,type=wc] ', (1, 10))
        ]
    )
    def test_pipe(self, pipe_cmd, expected_count, src_count):
        """
            This unit test checks piping other command into sources.
            Here is assumption that resulting number of sources should be lower than actual number of sources,
            but still large enough
        """
        res = process_commandline(cas_db, pipe_cmd + self.base_cmd + ' --count', ftdb)
        assert int(res) <= src_count
        assert expected_count[0] <= int(res) <= expected_count[1]


class TestModules:

    base_cmd = 'modules'

    @pytest.fixture
    def md_count(self):
        return int(process_commandline(cas_db, self.base_cmd + ' --count', ftdb))

    @pytest.mark.parametrize(
        'filter_cmd,expected_res,pathlike,path_pattern', [
            ('', (1, 500), False, None),
            (' --ftdb-filter=[path=*/net/*,type=wc]', (1, 100), True, "*/net/*"),
            (' --ftdb-filter=[has_global=*kvm*,type=wc]', (1, 100), False, None),
            (' --ftdb-filter=[has_func=*cpy,type=wc]', (100, 300), False, None),
            (' --ftdb-filter=[has_func=*alloc*,type=wc]and[has_func=memcpy,type=sp]', (100, 400), False, None),
            (' --ftdb-filter=[has_global=*kvm*,type=wc]and[has_global=sleep,type=sp]', (10, 60), False, None),
        ]
    )
    def test_filter_options(self, filter_cmd, expected_res, pathlike, path_pattern):
        res = process_commandline(cas_db, self.base_cmd + filter_cmd +' --count', ftdb)
        assert expected_res[0] <= int(res) < expected_res[1]

        if pathlike:
            res = process_commandline(cas_db, self.base_cmd + filter_cmd, ftdb)
            for path in res:
                assert fnmatch.fnmatch(path, path_pattern)

    @pytest.mark.parametrize(
        'keyword', [
            'name',
            'location'
        ]
    )
    def test_filter_unavailable(self, keyword):
        
        with pytest.raises(FilterException) as err:
            process_commandline(cas_db, self.base_cmd + f' --ftdb-filter=[{keyword}=abc,type=sp]', ftdb)

        assert f'Filter option "{keyword}" can be used' in str(err)

    @pytest.mark.parametrize(
        'pipe_cmd,expected_count', [
            ('globs ',(300, 400)),
            ('funcs ', (300, 400)),
            ('funcdecls ', (100, 500)),
            ('globs --ftdb-filter=[name=*kvm*,type=wc] ',(50, 100)),
            ('funcs --ftdb-filter=[name=*_desc,type=wc] ', (100, 300)),
            ('funcdecls --ftdb-filter=[name=*cpy,type=wc] ', (300, 400))
        ]
    )
    def test_pipe(self, pipe_cmd, expected_count, md_count):
        """
            This unit test checks piping other command into sources.
            Here is assumption that resulting number of sources should be lower than actual number of sources,
            but still large enough
        """
        res = process_commandline(cas_db, pipe_cmd + self.base_cmd + ' --count', ftdb)
        assert int(res) <= md_count
        assert expected_count[0] <= int(res) <= expected_count[1]

class TestFuncs:

    base_cmd = "funcs"

    @pytest.mark.parametrize(
        'filter_cmd,expected_res,pathlike,path_pattern', [
            ('', (150000, 200000), False, None),
            (' --ftdb-filter=[name=*cpy,type=wc]', (1, 50), False, None),
            (' --ftdb-filter=[location=*/soc/*,type=wc]', (5000, 15000), False, None)
        ]
    )
    def test_filter_options(self, filter_cmd, expected_res,pathlike,path_pattern):
        res = process_commandline(cas_db, self.base_cmd + filter_cmd +' --count', ftdb)
        assert expected_res[0] < int(res) < expected_res[1]

        if pathlike:
            res = process_commandline(cas_db, self.base_cmd + filter_cmd, ftdb)
            for path in res:
                assert fnmatch.fnmatch(path, path_pattern)

    @pytest.mark.parametrize(
        'keyword', [
            'path',
            'has_func',
            'has_global',
            'has_funcdecl'
        ]
    )
    def test_filter_unavailable(self, keyword):
        
        with pytest.raises(FilterException) as err:
            process_commandline(cas_db, self.base_cmd + f' --ftdb-filter=[{keyword}=abc,type=sp]', ftdb)

        assert f'Filter option "{keyword}" can be used' in str(err)
        
    def test_pipe(self):
        res = process_commandline(cas_db,'srcs --ftdb-filter=[path=*/kernel/irq/*,type=wc] ' + self.base_cmd +' --count', ftdb)
        assert 4000 < int(res) < 6000


class TestGlobs:

    base_cmd = "globs"

    @pytest.mark.parametrize(
        'filter_cmd,expected_res,pathlike,path_pattern', [
            ('', (10000, 110000), False, None),
            (' --ftdb-filter=[name=*kvm*,type=wc]', (500, 1000), False, None),
            (' --ftdb-filter=[location=*/soc/*,type=wc]', (5000, 11000), False, None)
        ]
    )
    def test_filter_options(self, filter_cmd, expected_res,pathlike,path_pattern):
        res = process_commandline(cas_db, self.base_cmd + filter_cmd +' --count', ftdb)
        assert expected_res[0] < int(res) < expected_res[1]

        if pathlike:
            res = process_commandline(cas_db, self.base_cmd + filter_cmd, ftdb)
            for path in res:
                assert fnmatch.fnmatch(path, path_pattern)


    @pytest.mark.parametrize(
        'keyword', [
            'path',
            'has_func',
            'has_global',
            'has_funcdecl'
        ]
    )
    def test_filter_unavailable(self, keyword):
        
        with pytest.raises(FilterException) as err:
            process_commandline(cas_db, self.base_cmd + f' --ftdb-filter=[{keyword}=abc,type=sp]', ftdb)

        assert f'Filter option "{keyword}" can be used' in str(err)

    def test_pipe(self):
        res = process_commandline(cas_db, 'srcs --ftdb-filter=[path=*/kernel/irq*,type=wc] '+self.base_cmd +' --count', ftdb)
        assert 100 < int(res) < 500

class TestFuncDecl:

    base_cmd = "funcdecls"

    @pytest.mark.parametrize(
        'filter_cmd,expected_res,pathlike,path_pattern', [
            ('', (1000, 10000), False, None),
            (' --ftdb-filter=[name=*cpy,type=wc]', (1, 1000), False, None),
            (' --ftdb-filter=[location=*/soc/*,type=wc]', (100, 500), False, None)
        ]
    )
    def test_filter_options(self, filter_cmd, expected_res,pathlike,path_pattern):
        res = process_commandline(cas_db, self.base_cmd + filter_cmd +' --count', ftdb)
        assert expected_res[0] < int(res) < expected_res[1]

        if pathlike:
            res = process_commandline(cas_db, self.base_cmd + filter_cmd, ftdb)
            for path in res:
                assert fnmatch.fnmatch(path, path_pattern)


    @pytest.mark.parametrize(
        'keyword', [
            'path',
            'has_func',
            'has_global',
            'has_funcdecl'
        ]
    )
    def test_filter_unavailable(self, keyword):
        
        with pytest.raises(FilterException) as err:
            process_commandline(cas_db, self.base_cmd + f' --ftdb-filter=[{keyword}=abc,type=sp]', ftdb)

        assert f'Filter option "{keyword}" can be used' in str(err)
    
    def test_pipe(self):
        res = process_commandline(cas_db, 'srcs --ftdb-filter=[path=*/kernel/irq/*,type=wc] ' + self.base_cmd +' --count', ftdb)
        assert 1 <= int(res) < 10

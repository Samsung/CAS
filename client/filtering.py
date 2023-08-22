from abc import abstractmethod
import re
from fnmatch import translate as translate_wc_to_re
from typing import List, Dict, Tuple, Optional, Any
import libetrace
from client.misc import get_file_info

from libcas import CASConfig


class Filter:
    """
    Filter class used to filter out nfsdbEntry or nfsdbEntryOpenfile based on provided filters.

    Filters can be provided both in string representation or object.

    string example:
    (path=/abs,type=wc)or(class=compiled)and(path=*.c,type=wc)
    """
    parameters_schema = {}
    typed_keywords = []

    def __init__(self, flt:"str | List[List[Dict[str, Any]]]", origin, config: CASConfig, source_root: str) -> None:
        self.filter_dict: List[List[Dict[str, Any]]] = flt if isinstance(flt, list) else self._filter_str_to_dict(flt)
        self.libetrace_filter = self.filter_to_libetrace(self.filter_dict)
        self.config = config
        self.source_root = source_root
        self.origin = origin

        for f_or in self.filter_dict:
            for f_and in f_or:
                if len(f_and) == 0:
                    raise FilterException("Empty filter!")
                for k in f_and.keys():
                    if k not in self.parameters_schema:
                        raise FilterException("Unknown filter parameter {} ! Allowed parameters: {}".format(k, ", ".join(self.parameters_schema.keys())))
                for k in [k for k in self.parameters_schema if k in f_and]:
                    if self.parameters_schema[k] is not None and f_and[k] not in self.parameters_schema[k]:
                        raise FilterException("'{}' is not proper '{}=' parameter value! Allowed values: {} ".format(f_and[k], k, ", ".join(self.parameters_schema[k])))

                typed_params = [x for x in ['path' in f_and, 'cwd' in f_and, 'bin' in f_and, 'cmd' in f_and] if x is True]
                if len(typed_params) > 1:
                    raise FilterException(f"More than one typed parameter! Choose between: {', '.join(self.typed_keywords)}")

                if 'type' in f_and and len(typed_params) == 0:
                    raise FilterException(f"'type' sub-parameter present but without associated typed parameter! Allowed values: {', '.join(self.typed_keywords)}")

                if 'type' not in f_and and len(typed_params) > 0:
                    f_and['type'] = "sp"

                if 'path' in f_and:
                    if f_and['type'] == "wc" or f_and['type'] == "sp":
                        if f_and['type'] == "sp":
                            f_and["path"] = f'*{f_and["path"]}*'
                        f_and['type'] = "re"
                        f_and['path_pattern'] = re.compile(translate_wc_to_re(f_and["path"]))
                    elif f_and['type'] == "re":
                        f_and['path_pattern'] = re.compile(f_and["path"])
                elif 'cwd' in f_and:
                    if f_and['type'] == "wc" or f_and['type'] == "sp":
                        if f_and['type'] == "sp":
                            f_and["cwd"] = f'*{f_and["cwd"]}*'
                        f_and['type'] = "re"
                        f_and['cwd_pattern'] = re.compile(translate_wc_to_re(f_and["cwd"]))
                    elif f_and['type'] == "re":
                        f_and['cwd_pattern'] = re.compile(f_and["cwd"])
                elif 'bin' in f_and:
                    if f_and['type'] == "wc" or f_and['type'] == "sp":
                        if f_and['type'] == "sp":
                            f_and["bin"] = f'*{f_and["bin"]}*'
                        f_and['type'] = "re"
                        f_and['bin_pattern'] = re.compile(translate_wc_to_re(f_and["bin"]))
                    elif f_and['type'] == "re":
                        f_and['bin_pattern'] = re.compile(f_and["bin"])
                elif 'cmd' in f_and:
                    if f_and['type'] == "wc" or f_and['type'] == "sp":
                        if f_and['type'] == "sp":
                            f_and["cmd"] = f'*{f_and["cmd"]}*'
                        f_and['type'] = "re"
                        f_and['cmd_pattern'] = re.compile(translate_wc_to_re(f_and["cmd"]))
                    elif f_and['type'] == "re":
                        f_and['cmd_pattern'] = re.compile(f_and["cmd"])

                if 'type' not in f_and and len(typed_params) > 0:
                    raise FilterException("'type' sub-parameter not present! Allowed values: {}".format(self.parameters_schema['type']))

                if 'type' in f_and and f_and['type'] == "wc":
                    assert False, "DEBUG! Some wildcards not cached to re! - this should never happen! {}".format(f_and)

                if 'class' in f_and and f_and['class'] == 'compiler':
                    self.comp_re = "|".join([x+"$" for x in self.config.config_info["gcc_spec"] + self.config.config_info["gpp_spec"] 
                                            + self.config.config_info["clang_spec"] + self.config.config_info["clangpp_spec"] + self.config.config_info["armcc_spec"]])
                    self.comp_re = self.comp_re.replace('+', r'\+').replace('.', r'\.').replace('/', r'\/').replace('*', '.*')
                    self.comp_re = re.compile(self.comp_re)
                if 'class' in f_and and f_and['class'] == 'linker':
                    self.link_re = "|".join([x+"$" for x in self.config.config_info["ld_spec"] + self.config.config_info["ar_spec"]])
                    self.link_re = self.link_re.replace('+', r'\+').replace('.', r'\.').replace('/', r'\/').replace('*', '.*')
                    self.link_re = re.compile(self.link_re)

                f_and["filter_class"] = 'class' in f_and
                f_and["filter_source_type"] = 'source_type' in f_and
                f_and["filter_access"] = 'access' in f_and
                f_and["filter_exists"] = 'exists' in f_and
                f_and["filter_link"] = 'link' in f_and
                f_and["filter_path"] = 'path' in f_and
                f_and["filter_source_root"] = 'source_root' in f_and
                f_and["filter_cmd"] = 'cmd' in f_and
                f_and["filter_cwd"] = 'cwd' in f_and
                f_and["filter_bin"] = 'bin' in f_and
                f_and["filter_ppid"] = 'pid' in f_and
                f_and["filter_negate"] = 'negate' in f_and

        self.ored = len(self.filter_dict) > 1
        self.anded = len([f_and for f_or in self.filter_dict for f_and in f_or]) > 1
        if self.origin and self.origin.args.debug:
            print(f"DEBUG: Filter dict:\n {self.filter_dict}")
        if self.origin and self.origin.args.debug:
            print(f"DEBUG: Etrace filter:\n {self.libetrace_filter}")

    @staticmethod
    def _process_part(filter_part: str) -> Dict:
        """
        Function converts single filter part into dict of filter values

        :param filter_part: filter part
        :type filter_part: str
        :raises FilterException: filter exception
        :return: dictionary with filter values
        :rtype: Dict
        """
        filter_part = filter_part.replace("(", "").replace(")", "")
        if len(filter_part) == 0:
            raise FilterException("Empty filter part!")
        ret = {}
        for filter_part in filter_part.split(","):
            if len(filter_part.split("=")) == 2:
                ret[filter_part.split("=")[0].strip()] = filter_part.split("=")[1].strip()
            else:
                ret["path"] = filter_part.strip()
        return ret

    @staticmethod
    def _filter_str_to_dict(filter_string: str) -> List[List[Dict[str, Any]]]:
        """
        Function splits filter into lists.
        Top level list contains all 'or'ed filter parts, second level lists contains 'and'ed filter parts.

        Example:
        (path=a)or(path=b)and(path=c)
        produces
        [ [ {"path": "a"} ], [ {"path": "b"}, {"path": "c"} ] ]

        :param filter_string: filter in string form
        :type filter_string: str
        :return: list of splitted filter parts
        :rtype: List
        """

        filter_string = filter_string.replace('[', '(').replace(']', ')').replace(')OR(', ')or(').replace(')AND(', ')and(')
        for _ in range(filter_string.count(" ")):
            filter_string = filter_string.replace("( ", "(").replace(" (", "(").replace(") ", ")").replace(" )", ")")
        return [[Filter._process_part(f) for f in ors.split(")and(")] if ")and(" in ors else [Filter._process_part(ors)] for ors in filter_string.split(")or(")]

    @staticmethod
    @abstractmethod
    def filter_to_libetrace(filter_dict: Optional[List]) -> List:
        """
        Function generate libetrace filter from parsed filter list.

        :param filter_dict: parsed filter
        :type filter_dict: Optional[List]
        :return: libetrace filters compatibile filter list
        :rtype: List
        """


class OpenFilter(Filter):
    """
    Filter specialized to deal with opens objects.
    """
    typed_keywords = ['path']
    parameters_schema: Dict[str, Optional[List[str]]] = {
            "class": ["linked", "linked_static", "linked_shared", "linked_exe", "compiled", "plain", "compiler", "linker", "binary"],
            "type": ["re", "wc", "sp"],
            "path": None,
            "access": ["r", "w", "rw"],
            "global_access": ["r", "w", "rw"],
            "exists": ["0", "1", "2"],
            "link": ["true", "false", "0", "1"],
            "source_root": ["true", "false", "0", "1"],
            "source_type": ["c", "c++", "other"],
            "negate": ["true", "false", "0", "1"]
        }

    def _match_filter(self, opn: libetrace.nfsdbEntryOpenfile, filter_part: Dict) -> bool:
        """
        Function check if open should pass filter part.

        :param opn: open file object
        :type opn: libetrace.nfsdbEntryOpenfile
        :param filter_part: filter part
        :type filter_part: dict
        :return: True if open matches filter part conditions otherwise False
        :rtype: bool
        """
        ret:bool = True

        if ret and filter_part["filter_class"]:
            if filter_part["class"] == "compiled":
                ret = ret and (opn.opaque is not None and opn.opaque.compilation_info is not None and opn.opaque.compilation_info.file_paths[0] == opn.path)
            elif filter_part["class"] == "linked":
                ret = ret and (opn.opaque is not None and opn.opaque.linked_file is not None and opn.opaque.linked_path == opn.path)
            elif filter_part["class"] == "linked_static":
                ret = ret and (opn.opaque is not None and opn.opaque.linked_file is not None and opn.opaque.linked_path == opn.path and opn.opaque.linked_type == 0)
            elif filter_part["class"] == "linked_shared":
                ret = ret and (opn.opaque is not None and opn.opaque.linked_file is not None and opn.opaque.linked_path == opn.path and opn.opaque.linked_type == 1 and ("-shared" in opn.opaque.argv or "--shared" in opn.opaque.argv))
            elif filter_part["class"] == "linked_exe":
                ret = ret and (opn.opaque is not None and opn.opaque.linked_file is not None and opn.opaque.linked_path == opn.path and opn.opaque.linked_type == 1 and not ("-shared" in opn.opaque.argv or "--shared" in opn.opaque.argv))
            elif filter_part["class"] == "compiler":
                ret = ret and (opn.opaque is not None and opn.opaque.compilation_info is not None)
            elif filter_part["class"] == "linker":
                ret = ret and (opn.opaque is not None and opn.opaque.linked_file is not None)
            elif filter_part["class"] == "plain":
                ret = ret and (opn.opaque is None)

        if ret and filter_part["filter_source_type"]:
            if opn.path in self.origin.get_src_types():
                if filter_part["source_type"] == "c":
                    ret = ret and (self.origin.get_src_types()[opn.path] == 1)
                elif filter_part["source_type"] == "c++":
                    ret = ret and (self.origin.get_src_types()[opn.path] == 2)
                elif filter_part["source_type"] == "other":
                    ret = ret and (self.origin.get_src_types()[opn.path] != 1 and self.origin.get_src_types()[opn.path] != 2)
            else:
                ret = ret and False

        if ret and filter_part["filter_access"]:
            exist, stat, mode = get_file_info(opn.mode)
            if filter_part["access"] == "r":
                ret = ret and (mode == 0)
            elif filter_part["access"] == "w":
                ret = ret and (mode == 1)
            elif filter_part["access"] == "rw":
                ret = ret and (mode == 2)

        if ret and filter_part["filter_exists"]:
            exist, stat, mode = get_file_info(opn.mode)
            if filter_part["exists"] == "1":
                ret = ret and (not stat == 2 and exist)
            elif filter_part["exists"] == "0":
                ret = ret and (not exist)
            elif filter_part["exists"] == "2":
                ret = ret and (stat == 2 and exist)

        if ret and filter_part["filter_link"]:
            exist, stat, mode = get_file_info(opn.mode)
            if filter_part["link"] == "1":
                ret = ret and (stat == 10)
            elif filter_part["link"] == "0":
                ret = ret and (not stat == 10)

        if ret and filter_part["filter_source_root"]:
            if filter_part["source_root"] == "1" or filter_part["source_root"] == "true":
                ret = ret and (opn.path.startswith(self.source_root))
            elif filter_part["source_root"] == "0" or filter_part["source_root"] == "false":
                ret = ret and (not opn.path.startswith(self.source_root))

        if ret and filter_part["filter_path"]:
            if "path_pattern" in filter_part:
                ret = ret and (filter_part['path_pattern'].match(opn.path) is not None)
            else:
                ret = ret and (filter_part["path"] in opn.path)

        if filter_part["filter_negate"]:
            ret = not ret

        return ret

    def resolve_filters(self, opn: libetrace.nfsdbEntryOpenfile) -> bool:
        """
        Function check if open should pass all filters. 

        :param opn: open file object
        :type opn: libetrace.nfsdbEntryOpenfile
        :return: True if open matches filters conditions otherwise False
        :rtype: bool
        """
        if not self.anded:
            return self._match_filter(opn, self.filter_dict[0][0])
        if self.ored:
            return any([all([self._match_filter(opn, f) for f in o]) for o in self.filter_dict])
        else:
            return all([self._match_filter(opn, f) for f in self.filter_dict[0]])

    @staticmethod
    def filter_to_libetrace(filter_dict: Optional[List]) -> List:
        # [ [(PATH,CLASS,EXISTS,ACCESS,NEGATE,SRCROOT,SRCTYPE),(...),...], ... ]

        file_class = {
            "linked" : 0x0001,
            "linked_static" : 0x0002,
            "linked_shared" : 0x0004,
            "linked_exe" : 0x0008,
            "compiled" : 0x0010,
            "plain" : 0x0020,
            "compiler" : 0x0040,
            "linker" : 0x0080,
            "binary" : 0x0100,
            "symlink" : 0x0200,
            "nosymlink" : 0x0400
        }

        def get_path(flt: Dict) -> Optional[Tuple[str,str]]:
            if "path" in flt:
                if "type" in flt and flt['type'] == "wc":
                    f_type = "matches_wc"
                elif "type"  in flt and flt['type'] == "re":
                    f_type = "matches_re"
                else:
                    f_type = "contains_path"

                return (f_type, flt['path'])
            return None

        def get_class(flt: Dict) -> Optional[Tuple[str,int]]:
            cls_vals:int = 0
            if "class" in flt:
                for cls_v in [file_class[c_val] for c_val in flt['class'].split("|")]:
                    cls_vals = cls_vals | cls_v

            if "link" in flt:
                if flt["link"] in ["true", "1"]:
                    cls_vals = cls_vals | file_class['symlink']
                elif flt["link"] in ["false", "0"]:
                    cls_vals = cls_vals | file_class['nosymlink']

            return ("is_class", cls_vals) if cls_vals != 0 else None

        def get_exists(flt: Dict) -> Optional[Tuple[str, Optional[bool]]]:
            if "exists" in flt:
                if flt["exists"] == "0":
                    return ("file_not_exists", None)
                elif flt["exists"] == "1":
                    return ("file_exists", None)
                elif flt["exists"] == "2":
                    return ("dir_exists", True)
            return None

        def get_access(flt: Dict) -> Optional[Tuple[str, int]]:
            if "access" in flt and "global_access" in flt:
                raise
            if "access" in flt:
                if flt["access"] == "r":
                    return "has_access", 0
                elif flt["access"] == "w":
                    return "has_access", 1
                elif flt["access"] == "rw":
                    return "has_access", 2
            if "global_access" in flt:
                if flt["global_access"] == "r":
                    return "has_access", 3
                elif flt["global_access"] == "w":
                    return "has_access", 4
                elif flt["global_access"] == "rw":
                    return "has_access", 5
            return None

        def get_src_root(flt: Dict) -> Optional[Tuple[str,None]]:
            if "source_root" in flt:
                if flt["source_root"] == "1":
                    return ("at_source_root", None)
                elif flt["source_root"] == "0":
                    return ("not_at_source_root", None)
            return None
        def get_src_type(flt: Dict) -> Optional[Tuple[str,int]]:
            if "source_type" in flt:
                if flt["source_type"] == "c":
                    return ("source_type", 1)
                elif flt["source_type"] == "c++":
                    return ("source_type", 2)
                elif flt["source_type"] == "other":
                    return ("source_type", 4)
            return None
        filter_list:List[List] = []

        if filter_dict:
            for i, or_list in enumerate(filter_dict):
                filter_list.append([])
                for j, and_dict in enumerate(or_list):
                    filter_list[i].append([])
                    # (PATH,CLASS,EXISTS,ACCESS,NEGATE,SRCROOT,SRCTYPE)
                    filter_list[i][j] = (
                        get_path(and_dict),
                        get_class(and_dict),
                        get_exists(and_dict),
                        get_access(and_dict),
                        True if "negate" in and_dict else False,
                        get_src_root(and_dict),
                        get_src_type(and_dict))
        return filter_list


class CommandFilter(Filter):
    """
    Filter specialized to deal with execs objects.
    """
    typed_keywords = ['bin','cwd','cmd']
    parameters_schema: Dict[str, Optional[List[str]]] = {
            "class": ["compiler", "linker", "command"],
            "type": ["re", "wc", "sp"],
            "cwd": None,
            "bin": None,
            "cmd": None,
            "ppid": None,
            "cwd_source_root": ["true", "false", "0", "1"],
            "bin_source_root": ["true", "false", "0", "1"],
            "negate": ["true", "false", "0", "1"]
        }

    def _match_filter(self, exe: libetrace.nfsdbEntry, filter_part: Dict) -> bool:
        """
        Function check if exec should pass filter part.

        :param exe: exec object
        :type exe: libetrace.nfsdbEntry
        :param filter_part: filter part
        :type filter_part: dict
        :return: True if exec matches filter part conditions otherwise False
        :rtype: bool
        """
        ret: bool = True

        if filter_part["filter_cwd"]:
            if "cwd_pattern" in filter_part:
                ret = ret and (False if not filter_part['cwd_pattern'].match(exe.cwd) else True)
            else:
                ret = ret and (exe.cwd == filter_part["cwd"])

        if filter_part["filter_cmd"]:
            if "cmd_pattern" in filter_part:
                ret = ret and (False if not filter_part['cmd_pattern'].match(" ".join(exe.argv)) else True)
            else:
                ret = ret and (" ".join(exe.argv) == filter_part["cmd"])

        if filter_part["filter_bin"]:
            if 'bin_pattern' in filter_part:
                ret = ret and (False if not filter_part['bin_pattern'].match(exe.bpath) else True)
            else:
                ret = ret and (exe.bpath == filter_part["bin"])

        if filter_part["filter_ppid"]:
            ret = ret and (int(exe.parent_eid.pid) == int(filter_part["ppid"]))

        if filter_part["filter_class"]:
            if filter_part["class"] == "compiler":
                ret = ret and exe.compilation_info is not None
            elif filter_part["class"] == "linker":
                ret = ret and exe.linked_file is not None
            elif filter_part["class"] == "command":
                ret = ret and (exe.bpath is not None)

        if filter_part["filter_source_root"]:
            if filter_part["source_root"] == "1":
                ret = ret and (exe.bpath.startswith(self.source_root))
            elif filter_part["source_root"] == "0":
                ret = ret and (not exe.bpath.startswith(self.source_root))

        if filter_part["filter_negate"]:
            ret = not ret

        return ret

    def resolve_filters(self, exe: libetrace.nfsdbEntry) -> bool:
        """
        Function check if exec should pass all filters.

        :param exe: exec object
        :type exe: libetrace.nfsdbEntryOpenfile
        :return: True if exec matches filters conditions otherwise False
        :rtype: bool
        """
        if not self.anded:
            return self._match_filter(exe, self.filter_dict[0][0])
        if self.ored:
            return any([all([self._match_filter(exe, f) for f in o]) for o in self.filter_dict])
        else:
            return all([self._match_filter(exe, f) for f in self.filter_dict[0]])

    @staticmethod
    def filter_to_libetrace(filter_dict: Optional[List]) -> List:
        # [ [(STR,PPID,CLASS,NEGATE,SRCROOT),(...),...], ... ]

        command_class = {
            "command" : 0x0001,
            "compiler" : 0x0002,
            "linker" : 0x0004,
        }

        def get_bin(flt: Dict) -> Optional[Tuple[str, str]]:
            if "bin" in flt:
                if "type" in flt and flt['type'] == "wc":
                    f_type = "bin_matches_wc"
                elif "type"  in flt and flt['type'] == "re":
                    f_type = "bin_matches_re"
                else:
                    f_type = "bin_contains_path"

                return (f_type, flt['bin'])
            if "cmd" in flt:
                if "type" in flt and flt['type'] == "wc":
                    f_type = "cmd_matches_wc"
                elif "type"  in flt and flt['type'] == "re":
                    f_type = "cmd_matches_re"
                else:
                    f_type = "cmd_has_string"

                return (f_type, flt['cmd'])
            if "cwd" in flt:
                if "type" in flt and flt['type'] == "wc":
                    f_type = "cwd_matches_wc"
                elif "type"  in flt and flt['type'] == "re":
                    f_type = "cwd_matches_re"
                else:
                    f_type = "cwd_contains_path"

                return (f_type, flt['cwd'])
            return None

        def get_ppid(flt: Dict) -> Optional[Tuple[str, int]]:
            if "ppid" in flt:
                return ("has_ppid", int(flt['ppid']))
            return None

        def get_class(flt: Dict) -> Optional[Tuple[str,int]]:
            cls_vals:int = 0
            if "class" in flt:
                for cls_v in [command_class[c_val] for c_val in flt['class'].split("|")]:
                    cls_vals = cls_vals | cls_v
            return ("is_class", cls_vals) if cls_vals != 0 else None

        def get_src_root(flt: Dict) -> Optional[Tuple[str,None]]:
            if "cwd_source_root" in flt:
                if flt["cwd_source_root"] == "1" or flt["cwd_source_root"] == "true":
                    return ("cwd_at_source_root", None)
                if flt["cwd_source_root"] == "0" or flt["cwd_source_root"] == "false":
                    return ("cwd_not_at_source_root", None)
            elif "bin_source_root" in flt:
                if flt["bin_source_root"] == "1" or flt["bin_source_root"] == "true":
                    return ("bin_at_source_root", None)
                if flt["bin_source_root"] == "0" or flt["bin_source_root"] == "false":
                    return ("bin_not_at_source_root", None)
            return None

        filter_list:List[List] = []
        if filter_dict:
            for i, or_list in enumerate(filter_dict):
                filter_list.append([])
                for j, and_dict in enumerate(or_list):
                    filter_list[i].append([])
                    # (PATH,CLASS,EXISTS,ACCESS,NEGATE,SRCROOT)
                    filter_list[i][j] = (
                        get_bin(and_dict),
                        get_ppid(and_dict),
                        get_class(and_dict),
                        True if "negate" in and_dict and (and_dict["negate"] == "1" or and_dict["negate"] == "true") else False,
                        get_src_root(and_dict))
        return filter_list


class FilterException(Exception):
    """
    Exception object used by `Filter` class

    :param message: Exception message
    :type message: str
    """
    def __init__(self, message: str):
        super(FilterException, self).__init__(message)
        self.message = message

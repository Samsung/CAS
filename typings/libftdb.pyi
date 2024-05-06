from _typeshed import Incomplete

class FtdbError(Exception): ...

class ftdb:
    BAS: Incomplete
    directory: Incomplete
    fops: Incomplete
    func_fptrs: Incomplete
    funcdecls: Incomplete
    funcs: Incomplete
    funcs_tree_calls_no_asm: Incomplete
    funcs_tree_calls_no_known: Incomplete
    funcs_tree_calls_no_known_no_asm: Incomplete
    funcs_tree_func_calls: Incomplete
    funcs_tree_func_refs: Incomplete
    funcs_tree_funrefs_no_asm: Incomplete
    funcs_tree_funrefs_no_known: Incomplete
    funcs_tree_funrefs_no_known_no_asm: Incomplete
    globals: Incomplete
    globs_tree_globalrefs: Incomplete
    init_data: Incomplete
    known_data: Incomplete
    module: Incomplete
    module_info: Incomplete
    modules: Incomplete
    release: Incomplete
    source_info: Incomplete
    sources: Incomplete
    static_funcs_map: Incomplete
    types: Incomplete
    types_tree_refs: Incomplete
    types_tree_usedrefs: Incomplete
    unresolvedfuncs: Incomplete
    version: Incomplete
    __refcount__: Incomplete
    def __init__(self, *args, **kwargs) -> None:
        """Initialize self.  See help(type(self)) for accurate signature."""
    def get_BAS_item_by_loc(self, *args, **kwargs):
        """Get the BAS_item based on its location"""
    def get_func_map_entry_by_id(self, *args, **kwargs):
        """Get the func_map_entry based on its id"""
    def load(self, *args, **kwargs):
        """Load the database cache file"""
    def __bool__(self) -> bool:
        """True if self else False"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __getitem__(self, index):
        """Return self[key]."""

class ftdbFops:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbFopsEntry:
    func: Incomplete
    kind: Incomplete
    loc: Incomplete
    members: Incomplete
    type: Incomplete
    var: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def json(self, *args, **kwargs):
        """Returns the json representation of the ftdb fops entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __deepcopy__(self):
        """Deep copy of an object"""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFopsIter:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""
    def __next__(self):
        """Implement next(self)."""

class ftdbFuncCallInfoEntry:
    args: Incomplete
    csid: Incomplete
    end: Incomplete
    expr: Incomplete
    loc: Incomplete
    ord: Incomplete
    start: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def json(self, *args, **kwargs):
        """Returns the json representation of the callinfo entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncDerefInfoEntry:
    access: Incomplete
    basecnt: Incomplete
    csid: Incomplete
    expr: Incomplete
    kind: Incomplete
    kindname: Incomplete
    mcall: Incomplete
    member: Incomplete
    offset: Incomplete
    offsetrefs: Incomplete
    ord: Incomplete
    ords: Incomplete
    shift: Incomplete
    type: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def json(self, *args, **kwargs):
        """Returns the json representation of the deref entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncEntry:
    asms: Incomplete
    attributes: Incomplete
    body: Incomplete
    call_info: Incomplete
    callrefs: Incomplete
    calls: Incomplete
    character_literals: Incomplete
    classOuterFn: Incomplete
    classid: Incomplete
    cshash: Incomplete
    csmap: Incomplete
    declbody: Incomplete
    declcount: Incomplete
    declhash: Incomplete
    decls: Incomplete
    derefs: Incomplete
    end_loc: Incomplete
    fid: Incomplete
    fids: Incomplete
    firstNonDeclStmt: Incomplete
    floating_literals: Incomplete
    funrefs: Incomplete
    globalrefInfo: Incomplete
    globalrefs: Incomplete
    hash: Incomplete
    id: Incomplete
    ifs: Incomplete
    inline: Incomplete
    integer_literals: Incomplete
    linkage: Incomplete
    linkage_string: Incomplete
    literals: Incomplete
    locals: Incomplete
    location: Incomplete
    macro_expansions: Incomplete
    member: Incomplete
    mids: Incomplete
    name: Incomplete
    namespace: Incomplete
    nargs: Incomplete
    refcall_info: Incomplete
    refcallrefs: Incomplete
    refcalls: Incomplete
    refcount: Incomplete
    refs: Incomplete
    signature: Incomplete
    start_loc: Incomplete
    string_literals: Incomplete
    switches: Incomplete
    taint: Incomplete
    template: Incomplete
    template_parameters: Incomplete
    types: Incomplete
    unpreprocessed_body: Incomplete
    variadic: Incomplete
    __refcount__: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def has_class(self, *args, **kwargs):
        """Returns True if func entry is contained within a class"""
    def has_classOuterFn(self, *args, **kwargs):
        """Returns True if func entry contains outer function class information"""
    def has_namespace(self, *args, **kwargs):
        """Returns True if func entry contains namespace information"""
    def json(self, *args, **kwargs):
        """Returns the json representation of the ftdb func entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __deepcopy__(self):
        """Deep copy of an object"""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __hash__(self) -> int:
        """Return hash(self)."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncIfInfoEntry:
    csid: Incomplete
    refs: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def json(self, *args, **kwargs):
        """Returns the json representation of the ifinfo entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncLocalInfoEntry:
    csid: Incomplete
    id: Incomplete
    location: Incomplete
    name: Incomplete
    parm: Incomplete
    static: Incomplete
    type: Incomplete
    used: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def has_csid(self, *args, **kwargs):
        """Returns True if func localinfo entry has the csid entry"""
    def json(self, *args, **kwargs):
        """Returns the json representation of the localinfo entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncOffsetrefInfoEntry:
    cast: Incomplete
    di: Incomplete
    id: Incomplete
    kind: Incomplete
    kindname: Incomplete
    mi: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def has_cast(self, *args, **kwargs):
        """Returns True if the offsetref entry contains cast information"""
    def json(self, *args, **kwargs):
        """Returns the json representation of the offsetref entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncSwitchInfoEntry:
    cases: Incomplete
    condition: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def json(self, *args, **kwargs):
        """Returns the json representation of the switchinfo entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncdeclEntry:
    classid: Incomplete
    decl: Incomplete
    declhash: Incomplete
    fid: Incomplete
    id: Incomplete
    linkage: Incomplete
    linkage_string: Incomplete
    location: Incomplete
    member: Incomplete
    name: Incomplete
    namespace: Incomplete
    nargs: Incomplete
    refcount: Incomplete
    signature: Incomplete
    template: Incomplete
    template_parameters: Incomplete
    types: Incomplete
    variadic: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def has_class(self, *args, **kwargs):
        """Returns True if funcdecl entry is contained within a class"""
    def has_namespace(self, *args, **kwargs):
        """Returns True if funcdecl entry contains namespace information"""
    def json(self, *args, **kwargs):
        """Returns the json representation of the ftdb funcdecl entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __deepcopy__(self):
        """Deep copy of an object"""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbFuncdecls:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def contains_hash(self, *args, **kwargs):
        """Check whether there is a funcdecl entry with a given hash"""
    def contains_id(self, *args, **kwargs):
        """Check whether there is a funcdecl entry with a given id"""
    def contains_name(self, *args, **kwargs):
        """Check whether there is a funcdecl entry with a given name"""
    def entry_by_hash(self, *args, **kwargs):
        """Returns the ftdb funcdecl entry with a given hash value"""
    def entry_by_id(self, *args, **kwargs):
        """Returns the ftdb funcdecl entry with a given id"""
    def entry_by_name(self, *args, **kwargs):
        """Returns the ftdb funcdecl entry with a given name"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbFuncs:
    __refcount__: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def contains_hash(self, *args, **kwargs):
        """Check whether there is a func entry with a given hash"""
    def contains_id(self, *args, **kwargs):
        """Check whether there is a func entry with a given id"""
    def contains_name(self, *args, **kwargs):
        """Check whether there is a func entry with a given name"""
    def entry_by_hash(self, *args, **kwargs):
        """Returns the ftdb func entry with a given hash value"""
    def entry_by_id(self, *args, **kwargs):
        """Returns the ftdb func entry with a given id"""
    def entry_by_name(self, *args, **kwargs):
        """Returns the list of ftdb func entries with a given name"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbFuncsIter:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""
    def __next__(self):
        """Implement next(self)."""

class ftdbGlobalEntry:
    character_literals: Incomplete
    decls: Incomplete
    defstring: Incomplete
    deftype: Incomplete
    fid: Incomplete
    floating_literals: Incomplete
    funrefs: Incomplete
    globalrefs: Incomplete
    hash: Incomplete
    hasinit: Incomplete
    id: Incomplete
    init: Incomplete
    integer_literals: Incomplete
    linkage: Incomplete
    linkage_string: Incomplete
    literals: Incomplete
    location: Incomplete
    mids: Incomplete
    name: Incomplete
    refs: Incomplete
    string_literals: Incomplete
    type: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def has_init(self, *args, **kwargs):
        """Returns True if global entry has init field"""
    def json(self, *args, **kwargs):
        """Returns the json representation of the ftdb global entry"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __deepcopy__(self):
        """Deep copy of an object"""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbGlobals:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def contains_hash(self, *args, **kwargs):
        """Check whether there is a global entry with a given hash"""
    def contains_id(self, *args, **kwargs):
        """Check whether there is a global entry with a given id"""
    def contains_name(self, *args, **kwargs):
        """Check whether there is a global entry with a given name"""
    def entry_by_hash(self, *args, **kwargs):
        """Returns the ftdb global entry with a given hash value"""
    def entry_by_id(self, *args, **kwargs):
        """Returns the ftdb global entry with a given id"""
    def entry_by_name(self, *args, **kwargs):
        """Returns the list of ftdb global entries with a given name"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbGlobalsIter:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""
    def __next__(self):
        """Implement next(self)."""

class ftdbModules:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbModulesIter:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""
    def __next__(self):
        """Implement next(self)."""

class ftdbSources:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbSourcesIter:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""
    def __next__(self):
        """Implement next(self)."""

class ftdbTypeEntry:
    attrnum: Incomplete
    attrrefs: Incomplete
    bitfields: Incomplete
    classid: Incomplete
    classname: Incomplete
    decls: Incomplete
    defhead: Incomplete
    defstring: Incomplete
    dependent: Incomplete
    enumvalues: Incomplete
    fid: Incomplete
    funrefs: Incomplete
    globalrefs: Incomplete
    hash: Incomplete
    id: Incomplete
    identifiers: Incomplete
    implicit: Incomplete
    isunion: Incomplete
    location: Incomplete
    memberoffsets: Incomplete
    methods: Incomplete
    name: Incomplete
    outerfn: Incomplete
    outerfnid: Incomplete
    qualifiers: Incomplete
    refcount: Incomplete
    refnames: Incomplete
    refs: Incomplete
    size: Incomplete
    str: Incomplete
    union: Incomplete
    useddef: Incomplete
    usedrefs: Incomplete
    values: Incomplete
    variadic: Incomplete
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def has_attrs(self, *args, **kwargs):
        """Returns True if type entry has attribute references"""
    def has_decls(self, *args, **kwargs):
        """Returns True if type entry has decls field"""
    def has_location(self, *args, **kwargs):
        """Returns True if type entry has location field"""
    def has_outerfn(self, *args, **kwargs):
        """Returns True if type entry has outerfn field"""
    def has_usedrefs(self, *args, **kwargs):
        """Returns True if type entry has usedrefs field"""
    def isConst(self, *args, **kwargs):
        """Check whether the type is a const type"""
    def json(self, *args, **kwargs):
        """Returns the json representation of the ftdb type entry"""
    def toNonConst(self, *args, **kwargs):
        """Returns the non-const counterpart of a type (if available)"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __deepcopy__(self):
        """Deep copy of an object"""
    def __eq__(self, other: object) -> bool:
        """Return self==value."""
    def __ge__(self, other: object) -> bool:
        """Return self>=value."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __gt__(self, other: object) -> bool:
        """Return self>value."""
    def __hash__(self) -> int:
        """Return hash(self)."""
    def __le__(self, other: object) -> bool:
        """Return self<=value."""
    def __lt__(self, other: object) -> bool:
        """Return self<value."""
    def __ne__(self, other: object) -> bool:
        """Return self!=value."""

class ftdbTypes:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def contains_hash(self, *args, **kwargs):
        """Check whether there is a types entry with a given hash"""
    def contains_id(self, *args, **kwargs):
        """Check whether there is a types entry with a given id"""
    def entry_by_hash(self, *args, **kwargs):
        """Returns the ftdb types entry with a given hash value"""
    def entry_by_id(self, *args, **kwargs):
        """Returns the ftdb types entry with a given id"""
    def __contains__(self, other) -> bool:
        """Return key in self."""
    def __getitem__(self, index):
        """Return self[key]."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbTypesIter:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""
    def __next__(self):
        """Implement next(self)."""

class ftdbUnresolvedfuncs:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __deepcopy__(self):
        """Deep copy of an object"""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""

class ftdbUnresolvedfuncsIter:
    @classmethod
    def __init__(cls, *args, **kwargs) -> None:
        """Create and return a new object.  See help(type) for accurate signature."""
    def __iter__(self):
        """Implement iter(self)."""
    def __len__(self) -> int:
        """Return len(self)."""
    def __next__(self):
        """Implement next(self)."""

def create_ftdb(*args, **kwargs):
    """Create cached version of Function/Type database file"""
def parse_c_fmt_string(*args, **kwargs):
    """Parse C format string and returns a list of types of detected parameters"""

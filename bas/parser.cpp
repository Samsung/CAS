#include "parser.hpp"

/* Refactored based on SimpleJSON */
std::string json_escape(const std::string &input) {
    std::string result;
    for (size_t i = 0; i < input.length(); ++i) {
        switch (input[i]) {
        case '\"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
        {
            if ((static_cast<unsigned>(input[i]) < 0x80) && (static_cast<unsigned>(input[i]) > 0x1F))
            {
                result += input[i];
            }
            break;
        }
        }
    }
    return result;
}

Errorable<void> StreamParser::parse_line(const char *line, size_t size, uint64_t line_number) {
    auto &_process_map = process_map();
    auto &_stats_collector = stats_collector();
    eventTuple_t event_tuple;

    if (size < 4 || std::strncmp(line, "0: ", 3))
        return BadFormatError(line_number);

    auto ret = parse_generic_args(line + 3, line_number);

    if (ret.is_error())
        return ret;

    event_tuple = ret.value();

    set_last_event_time(event_tuple.timestamp);

    if (!first_event_time())
        set_first_event_time(event_tuple.timestamp);

    auto map_lookup = _process_map.find(event_tuple.pid);

    if (event_tuple.tag == Tag::ContEnd)
        _stats_collector.increment_multilines();

    if (map_lookup == _process_map.end())
        _process_map.insert(std::make_pair(event_tuple.pid, Process(event_tuple.pid, event_tuple)));
    else
        map_lookup->second.event_list.push_back(event_tuple);

    schedule_processing(event_tuple);

    return {};
}

void CachingParser::schedule_processing(eventTuple_t &event) {
    auto &_cache = cache();
    auto &_process_map = process_map();
    auto &_pending_exits = pending_exits();
    auto _lifetime = cache_lifetime();

    _pending_exits.insert(event.pid);

    if (event.tag == Tag::Exit) {
        _cache.push_back(CacheEntry(event.pid, event.line_number + _lifetime));
        _pending_exits.erase(event.pid);
    }

    if (!_cache.size())
        return;

    auto &entry = _cache.front();

    if (entry.hitcount == event.line_number) {
        auto &process = _process_map.at(entry.pid);
        cache_hit(process);
        _cache.erase(_cache.begin());
    }
}

void CachingParser::cache_hit(Process &process, bool pending) {
    std::sort(process.event_list.begin(), process.event_list.end(),
            [] (const eventTuple_t &a, const eventTuple_t &b) {
                return a.timestamp < b.timestamp;
            });
    process.first_event_time = process.event_list.front().timestamp;

    if (pending)
        process.last_event_time = last_event_time();
    else
        process.last_event_time = process.event_list.back().timestamp;

    trigger_parsing(process);
}

void CachingParser::cleanup_cache(void) {
    auto &_cache = cache();
    auto &_process_map = process_map();
    auto &_pending_exits = pending_exits();
    auto &_stats_collector = stats_collector();

    for (auto it = _cache.begin(); it != _cache.end(); ) {
        auto &cached_entry = *it;
        auto &process = _process_map.at(cached_entry.pid);
        cache_hit(process);
        it = _cache.erase(it);
        _stats_collector.increment_procs_at_exit();
    }

    for (auto it = _pending_exits.begin(); it != _pending_exits.end(); ) {
        auto &pid  = *it;
        auto &process = _process_map.at(pid);
        cache_hit(process, true);
        it = _pending_exits.erase(it);
        _stats_collector.increment_procs_at_exit();
    }
}

Errorable<NewProcArguments> StreamParser::parse_newproc_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    uint64_t value;
    ShortArguments tag;
    NewProcArguments arguments = {};

    for (;;) {
        separator = std::strchr(event_line, '=');
        if (!separator)
            return BadSeparatorError(event.line_number, event_line);

        tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
        switch (tag) {
        case ShortArguments::ArgSize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);

            arguments.argsize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Prognameisize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);

            arguments.prognameisize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Prognamepsize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);

            arguments.prognamepsize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::CwdSize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != 0) [[unlikely]]
                return BadSeparatorError(event.line_number, separator);

            arguments.cwdsize = value;
            goto ret;
        default:
            return UnexpectedTagError(event.line_number, tag);
        }
    }

ret:
    return arguments;
}

Errorable<SchedForkArguments> StreamParser::parse_schedfork_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    ShortArguments tag;
    SchedForkArguments arguments;

    separator = std::strchr(event_line, '=');
    if (!separator)
        return BadSeparatorError(event.line_number, event_line);

    tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
    if (tag != ShortArguments::Pid)
        return UnexpectedTagError(event.line_number, tag);

    errno = 0;
    arguments.cpid = std::strtoull(++separator, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(event.line_number, separator, errno);
    else if (*endptr != 0) [[unlikely]]
        return BadSeparatorError(event.line_number, separator);

    return arguments;
}

Errorable<SysCloneArguments> StreamParser::parse_sysclone_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    ShortArguments tag;
    SysCloneArguments arguments;

    separator = std::strchr(event_line, '=');
    if (!separator)
        return BadSeparatorError(event.line_number, event_line);

    tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
    if (tag != ShortArguments::Flags)
        return UnexpectedTagError(event.line_number, tag);

    errno = 0;
    arguments.flags = std::strtoull(++separator, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(event.line_number, separator, errno);
    else if (*endptr != 0) [[unlikely]]
        return BadSeparatorError(event.line_number, separator);

    return arguments;
}

Errorable<OpenArguments> StreamParser::parse_open_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    uint64_t value;
    ShortArguments tag;
    OpenArguments arguments = {};

    for (;;) {
        separator = std::strchr(event_line, '=');
        if (!separator)
            return BadSeparatorError(event.line_number, event_line);

        tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
        switch (tag) {
        case ShortArguments::Fnamesize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno)
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.fnamesize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Forigsize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno)
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.forigsize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Flags:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno)
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.flags = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Mode:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno)
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.mode = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Fd:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno)
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != 0) [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.fd = value;
            goto ret;
        default:
            return UnexpectedTagError(event.line_number, tag);
        }
    }

ret:
    return arguments;
}

Errorable<CloseArguments> StreamParser::parse_close_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    ShortArguments tag;
    CloseArguments arguments;

    separator = std::strchr(event_line, '=');
    if (!separator)
        return BadSeparatorError(event.line_number, event_line);

    tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
    if (tag != ShortArguments::Fd)
        return UnexpectedTagError(event.line_number, tag);

    errno = 0;
    arguments.fd = std::strtoll(++separator, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(event.line_number, separator, errno);
    else if (*endptr != 0) [[unlikely]]
        return BadSeparatorError(event.line_number, separator);

    return arguments;
}

Errorable<PipeArguments> StreamParser::parse_pipe_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    uint64_t value;
    ShortArguments tag;
    PipeArguments arguments = {};

    for (;;) {
        separator = std::strchr(event_line, '=');
        if (!separator)
            return BadSeparatorError(event.line_number, event_line);

        tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
        switch (tag) {
        case ShortArguments::Fd1: {
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.fd1 = value;
            event_line = ++endptr;
            break;
        }
        case ShortArguments::Fd2: {
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.fd2 = value;
            event_line = ++endptr;
            break;
        }
        case ShortArguments::Flags: {
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != 0) [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.flags = value;
            event_line = ++endptr;
            goto ret;
        }
        default:
            return UnexpectedTagError(event.line_number, tag);
        }
    }

ret:
    return arguments;
}

Errorable<RenameArguments> StreamParser::parse_rename_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    ShortArguments tag;
    RenameArguments arguments = {};

    separator = std::strchr(event_line, '=');
    if (!separator)
        return BadSeparatorError(event.line_number, event_line);

    tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
    if (tag != ShortArguments::Fnamesize)
        return UnexpectedTagError(event.line_number, tag);

    errno = 0;
    arguments.fnamesize = std::strtoull(++separator, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(event.line_number, separator, errno);
    else if (*endptr != 0) [[unlikely]]
        return BadSeparatorError(event.line_number, separator);

    return arguments;
}

Errorable<LinkArguments> StreamParser::parse_link_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    ShortArguments tag;
    LinkArguments arguments = {};

    separator = std::strchr(event_line, '=');
    if (!separator)
        return BadSeparatorError(event.line_number, event_line);

    tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
    if (tag != ShortArguments::Fnamesize)
        return UnexpectedTagError(event.line_number, tag);

    errno = 0;
    arguments.fnamesize = std::strtoull(++separator, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(event.line_number, separator, errno);
    else if (*endptr != 0) [[unlikely]]
        return BadSeparatorError(event.line_number, separator);

    return arguments;
}

Errorable<ExitArguments> StreamParser::parse_exit_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    ShortArguments tag;
    ExitArguments arguments;

    separator = std::strchr(event_line, '=');
    if (!separator)
        return BadSeparatorError(event.line_number, event_line);

    tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
    if (tag != ShortArguments::Status)
        return UnexpectedTagError(event.line_number, tag);

    errno = 0;
    arguments.status = std::strtol(++separator, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(event.line_number, separator, errno);
    else if (*endptr != 0) [[unlikely]]
        return BadSeparatorError(event.line_number, separator);

    return arguments;
}

Errorable<RenameArguments> StreamParser::parse_rename2_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    uint64_t value;
    ShortArguments tag;
    RenameArguments arguments = {};

    separator = std::strchr(event_line, '=');
    if (!separator)
        return BadSeparatorError(event.line_number, event_line);

    tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
    switch (tag) {
    case ShortArguments::Fnamesize:
        errno = 0;
        value = std::strtoull(++separator, &endptr, 10);
        if (errno) [[unlikely]]
            return IntegerParseError(event.line_number, separator, errno);
        else if (*endptr != ',') [[unlikely]]
            return BadSeparatorError(event.line_number, separator);
        arguments.fnamesize = value;
        event_line = ++endptr;
        break;
    case ShortArguments::Flags:
        errno = 0;
        value = std::strtoull(++separator, &endptr, 10);
        if (errno) [[unlikely]]
            return IntegerParseError(event.line_number, separator, errno);
        else if (*endptr != 0) [[unlikely]]
            return BadSeparatorError(event.line_number, separator);
        arguments.flags = value;
        goto ret;
    default:
        return UnexpectedTagError(event.line_number, tag);
    }

ret:
    return arguments;
}

Errorable<LinkArguments> StreamParser::parse_linkat_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    uint64_t value;
    ShortArguments tag;
    LinkArguments arguments = {};

    for (;;) {
        separator = std::strchr(event_line, '=');
        if (!separator)
            return BadSeparatorError(event.line_number, event_line);

        tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
        switch (tag) {
        case ShortArguments::Fnamesize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.fnamesize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Flags:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != 0) [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.flags = value;
            goto ret;
        default:
            return UnexpectedTagError(event.line_number, tag);
        }
    }

ret:
    return arguments;
}

Errorable<DupArguments> StreamParser::parse_dup_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    uint64_t value;
    ShortArguments tag;
    DupArguments arguments = {};

    for (;;) {
        separator = std::strchr(event_line, '=');
        if (!separator)
            return BadSeparatorError(event.line_number, event_line);

        tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
        switch (tag) {
        case ShortArguments::Oldfd:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.oldfd = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Newfd:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.newfd = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Flags:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != 0) [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.flags = value;
            goto ret;
        default:
            return UnexpectedTagError(event.line_number, tag);
        }
    }

ret:
    return arguments;
}

Errorable<SymlinkArguments> StreamParser::parse_symlink_short_arguments(const eventTuple_t &event) {
    char *endptr;
    char *separator;
    char *event_line = const_cast<char *>(event.event_arguments.c_str());
    uint64_t value;
    ShortArguments tag;
    SymlinkArguments arguments = {};
    arguments.resolvednamesize = -1;

    for (;;) {
        separator = std::strchr(event_line, '=');
        if (!separator)
            return BadSeparatorError(event.line_number, event_line);

        tag = static_cast<ShortArguments>(hash(event_line, separator - event_line));
        switch (tag) {
        case ShortArguments::Targetnamesize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.targetnamesize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Resolvednamesize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.resolvednamesize = value;
            event_line = ++endptr;
            break;
        case ShortArguments::Linknamesize:
            errno = 0;
            value = std::strtoull(++separator, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(event.line_number, separator, errno);
            else if (*endptr != 0) [[unlikely]]
                return BadSeparatorError(event.line_number, separator);
            arguments.linknamesize = value;
            goto ret;
        default:
            return UnexpectedTagError(event.line_number, tag);
        }
    }

ret:
    return arguments;
}

size_t StreamParser::parse_arguments(std::vector<eventTuple_t>::iterator &it,
                                     const std::vector<eventTuple_t>::iterator &end_it,
                                     std::vector<std::string> &result)
{
    ssize_t last_argument = NOT_INDEXED;
    size_t size = 0;

    for (;;) {
        auto &event = *it;

        if (it == end_it)
            break;

        if (event.tag != Tag::Cont && event.tag != Tag::ContEnd && event.tag != Tag::ArrayedArguments)
            break;

        if (event.tag == Tag::Cont) {
            auto &previous_argument = result.back();
            previous_argument.append("\\n" + json_escape(event.event_arguments));
            size += event.event_arguments.size() + 1;
            it++;
            continue;
        }

        if (event.tag == Tag::ContEnd) {
            it++;
            continue;
        }

        if (last_argument == event.argument_index) {
            auto &previous_argument = result.back();
            previous_argument.append(json_escape(event.event_arguments));
            size += event.event_arguments.size();
            it++;
            continue;
        } else if (last_argument + 1 != event.argument_index) {
            size++;
        }

        if (event.tag == Tag::ArrayedArguments)
            last_argument = event.argument_index;

        result.push_back(json_escape(event.event_arguments));
        size += event.event_arguments.size() + 1;
        it++;
    }

    return size;
}

size_t StreamParser::parse_long_argument(std::vector<eventTuple_t>::iterator &it,
        const std::vector<eventTuple_t>::iterator &end_it, Tag begin, Tag extended, Tag end,
        std::string &result)
{
    size_t size = 0;

    for (;;) {
        auto &event = *it;

        if (it == end_it)
            break;

        if (event.tag != begin && event.tag != extended && event.tag != end && event.tag != Tag::Cont && event.tag != Tag::ContEnd)
            break;

        if (event.tag == end) {
            it++;
            break;
        }

        if (event.tag == Tag::Cont) {
            result.append('\n' + json_escape(event.event_arguments));
            size += event.event_arguments.size() + 1;
            it++;
            continue;
        }

        if (event.tag == Tag::ContEnd) {
            it++;
            continue;
        }

        if (event.tag == begin) {
            result = json_escape(event.event_arguments);
            size += event.event_arguments.size();
            it++;
            continue;
        }

        result.append(json_escape(event.event_arguments));
        size += event.event_arguments.size();
        it++;
    }

    return size;
}

Errorable<eventTuple_t> StreamParser::parse_generic_args(const char *line, size_t line_number) {
    char *event_separator;
    char *argument_bracket;
    char *endptr;
    ssize_t argument_index = NOT_INDEXED;
    unsigned long long pid;
    unsigned long cpu;
    unsigned long time;
    unsigned long timen;
    eventTuple_t event;
    Tag tag = Tag::None;

    errno = 0;
    pid = std::strtoull(line, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(line_number, line, errno);
    else if (*endptr != ',') [[unlikely]]
        return BadSeparatorError(line_number, line);
    line = ++endptr;

    cpu = std::strtoul(line, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(line_number, line, errno);
    else if (*endptr != ',') [[unlikely]]
        return BadSeparatorError(line_number, line);
    line = ++endptr;

    time = std::strtoul(line, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(line_number, line, errno);
    else if (*endptr != ',') [[unlikely]]
        return BadSeparatorError(line_number, line);
    line = ++endptr;

    timen = std::strtoul(line, &endptr, 10);
    if (errno) [[unlikely]]
        return IntegerParseError(line_number, line, errno);
    else if (*endptr != '!') [[unlikely]]
        return BadSeparatorError(line_number, line);
    line = ++endptr;

    event_separator = const_cast<char *>(std::strchr(line, '|'));
    argument_bracket = const_cast<char *>(std::strchr(line, '['));

    if (!event_separator && !argument_bracket) {
        return BadSeparatorError(line_number, line);
    } else if ((event_separator && argument_bracket && event_separator > argument_bracket) || (!event_separator && argument_bracket)) {
        errno = 0;
        argument_index = std::strtoull(argument_bracket + 1, &endptr, 10);
        if (errno) [[unlikely]]
            return IntegerParseError(line_number, line, errno);
        else if (*endptr != ']') [[unlikely]]
            return BadSeparatorError(line_number, line);

        tag = static_cast<Tag>(hash(line, (argument_bracket + 1) - line));
        if (tag != Tag::ArrayedArguments)
           return UnexpectedArgumentEndError(line_number);
    } else if ((event_separator && argument_bracket && event_separator < argument_bracket) || (event_separator && !argument_bracket)) {
        tag = static_cast<Tag>(hash(line, event_separator - line));
        switch (tag) {
        case Tag::ProgramInterpreterExtended:
        case Tag::ProgramPathExtended:
        case Tag::CurrentWorkingDirectoryExtended:
        case Tag::AbsolutePathExtended:
        case Tag::OriginalPathExtended:
        case Tag::RenameFromPathExtended:
        case Tag::RenameToPathExtended:
        case Tag::LinkFromPathExtended:
        case Tag::LinkToPathExtended:
        case Tag::SymlinkTargetNameExtended:
        case Tag::SymlinkTargetPathExtended:
        case Tag::SymlinkPathExtended:
            argument_bracket = std::strchr(event_separator + 1, '[');
            if (!argument_bracket)
                return BadSeparatorError(line_number, event_separator + 1);

            errno = 0;
            argument_index = std::strtoull(argument_bracket + 1, &endptr, 10);
            if (errno) [[unlikely]]
                return IntegerParseError(line_number, line, errno);
            else if (*endptr != ',') [[unlikely]]
                return BadSeparatorError(line_number, line);
            break;
        default:
            break;
        }
    }

    event.pid = pid;
    event.cpu = cpu;
    event.tag = tag;
    event.argument_index = argument_index;
    event.timestamp = time * 1000000000UL + timen;
    event.line_number = line_number;
    event.event_arguments = tag == Tag::ArrayedArguments ? ++endptr : ++event_separator;

    return event;
}

Errorable<void> StreamParser::process_events(Process &process) {
    long n_processed = 0;
    long n_execs = 0;
    uint64_t exe_start_time = process.first_event_time;
    process.executions.back().timestamp = exe_start_time - first_event_time();
    size_t size = 0;
    auto& _stats_collector = stats_collector();

    auto bound_check_iter = [](std::vector<eventTuple_t>::iterator &it, std::vector<eventTuple_t>::iterator &end_it) {
        if (it == end_it) {
            format_print("unexpected end of arguments, last one on line ", ((*(it - 1)).line_number));
            exit(EXIT_FAILURE);
        }
    };

    auto increment_bound_check_iter = [&](std::vector<eventTuple_t>::iterator &it, std::vector<eventTuple_t>::iterator &end_it) {
        bound_check_iter(it, end_it);
        it++;
        bound_check_iter(it, end_it);
    };

    auto end_it = process.event_list.end();
    for (auto it = process.event_list.begin(); it != end_it; ) {
        auto &evln = *it;
        switch (evln.tag) {
        case Tag::NewProc: {
            std::string program_interpreter;
            NewProcArguments argnfo = {};
            Execution execution;
            auto result = StreamParser::parse_newproc_short_arguments(evln);
            if (result.is_error())
                return result;

            argnfo = result.value();

            increment_bound_check_iter(it, end_it);

            auto last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::ProgramInterpreter,
                    Tag::ProgramInterpreterExtended, Tag::ProgramInterpreterEnd, program_interpreter);

            bound_check_iter(it, end_it);

            if (size != argnfo.prognameisize)
                return SizeMismatchError(last_it->line_number, size, argnfo.prognameisize);

            last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::ProgramPath,
                    Tag::ProgramPathExtended, Tag::ProgramPathEnd, execution.program_path);

            bound_check_iter(it, end_it);

            if (size != argnfo.prognamepsize)
                return SizeMismatchError(last_it->line_number, size, argnfo.prognamepsize);

            last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::CurrentWorkingDirectory,
                    Tag::CurrentWorkingDirectoryExtended, Tag::CurrentWorkingDirectoryEnd,
                    execution.current_working_directory);

            bound_check_iter(it, end_it);

            if (size != argnfo.cwdsize)
                return SizeMismatchError(last_it->line_number, size, argnfo.cwdsize);

            last_it = it;
            size = StreamParser::parse_arguments(it, end_it, execution.arguments);

            bound_check_iter(it, end_it);

            if ((*it).tag != Tag::EndOfArgs)
                return UnexpectedArgumentEndError((*it).line_number);

            it++;

            auto &previous_execution = process.executions.back();
            previous_execution.elapsed_time = evln.timestamp - exe_start_time;

            execution.pid = evln.pid;
            execution.index = previous_execution.index + 1;
            process.executions.push_back(execution);

            exe_start_time = evln.timestamp;
            process.executions.back().timestamp = exe_start_time - first_event_time();

            append_syscall(syscall_raw(process.pid, syscall_raw::SYS_EXEC,
                        evln.timestamp - first_event_time()));

            n_processed++;
            n_execs++;
            _stats_collector.increment_exec();

            break;
        } break;
        case Tag::SchedFork: {
            SchedForkArguments argnfo = {};
            auto result = StreamParser::parse_schedfork_short_arguments(evln);
            if (result.is_error())
                return result;
            it++;
            argnfo = result.value();
            Execution &e = process.executions.back();
            e.children.push_back(Child(argnfo.cpid, 0));

            register_child(argnfo.cpid, evln.pid, process.executions.size() - 1);
            n_processed++;
            _stats_collector.increment_fork();
            append_syscall(syscall_raw(process.pid, syscall_raw::SYS_FORK, argnfo.cpid, 0,
                        evln.timestamp - first_event_time()));
        } break;
        case Tag::SysClone: {
            SysCloneArguments argnfo = {};
            auto result = StreamParser::parse_sysclone_short_arguments(evln);
            if (result.is_error())
                return result;
            argnfo = result.value();

            /* There should be SchedFork or SysCloneFailed event following Clone */
            ++it;
            bound_check_iter(it, end_it);
            auto &ievln = *it;
            if ((ievln.tag != Tag::SchedFork) && (ievln.tag != Tag::SysCloneFailed))
                return {};
            if (ievln.tag == Tag::SysCloneFailed) {
                /* Nothing to do, just keep going */
                it++;
            } else {
                SchedForkArguments argnfo_fork = {};
                auto result = StreamParser::parse_schedfork_short_arguments(ievln);
                if (result.is_error())
                    return result;
                it++;
                argnfo_fork = result.value();
                Execution &e = process.executions.back();

                e.children.push_back(Child(argnfo_fork.cpid, argnfo.flags));

                register_child(argnfo_fork.cpid, evln.pid, process.executions.size() - 1);
                n_processed += 2;
                _stats_collector.increment_fork();
                append_syscall(syscall_raw(process.pid, syscall_raw::SYS_FORK, argnfo_fork.cpid,
                            argnfo.flags, evln.timestamp - first_event_time()));
            }
        } break;
        case Tag::Close: {
            CloseArguments argnfo = {};
            auto result = StreamParser::parse_close_short_arguments(evln);
            if (result.is_error())
                return result;
            argnfo = result.value();
            n_processed++;
            _stats_collector.increment_close();
            append_syscall(syscall_raw(process.pid, syscall_raw::SYS_CLOSE, argnfo.fd, -1, 0,
                        evln.timestamp - first_event_time()));
            it++;
        } break;
        case Tag::Pipe: {
            PipeArguments argnfo = {};
            auto result = StreamParser::parse_pipe_short_arguments(evln);
            if (result.is_error())
                return result;
            argnfo = result.value();
            n_processed++;
            _stats_collector.increment_pipe();
            append_syscall(syscall_raw(process.pid, syscall_raw::SYS_PIPE, argnfo.fd1, argnfo.fd2,
                        argnfo.flags, evln.timestamp - first_event_time()));
            it++;
        } break;
        case Tag::Dup: {
            DupArguments argnfo = {};
            auto result = StreamParser::parse_dup_short_arguments(evln);
            if (result.is_error())
                return result;
            argnfo = result.value();
            n_processed++;

            _stats_collector.increment_dup();
            append_syscall(syscall_raw(process.pid, syscall_raw::SYS_DUP, argnfo.oldfd,
                        argnfo.newfd, argnfo.flags, evln.timestamp - first_event_time()));
            it++;
        } break;
        case Tag::Open: {
            OpenArguments argnfo = {};
            auto opened_file = OpenFile();
            std::string absolute_path;
            std::string original_path;
            auto result = StreamParser::parse_open_short_arguments(evln);
            if (result.is_error())
                return result;
            argnfo = result.value();

            ++it;
            bound_check_iter(it, end_it);

            auto last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::AbsolutePath,
                    Tag::AbsolutePathExtended, Tag::AbsolutePathEnd, absolute_path);

            bound_check_iter(it, end_it);

            if (size != argnfo.fnamesize)
                return SizeMismatchError(last_it->line_number, size, argnfo.fnamesize);

            last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::OriginalPath,
                    Tag::OriginalPathExtended, Tag::OriginalPathEnd, original_path);

            if (size != argnfo.forigsize)
                return SizeMismatchError(last_it->line_number, size, argnfo.forigsize);

            Execution &execution = process.executions.back();

            opened_file.mode = argnfo.flags & 0x03;
            opened_file.absolute_path = std::move(absolute_path);

            execution.add_open_file(original_path, opened_file);

            n_processed++;
            _stats_collector.increment_open();
        } break;
        case Tag::RenameFrom:
        case Tag::Rename2From: {
            std::string from_path;
            std::string to_path;
            auto from = OpenFile();
            auto to = OpenFile();
            RenameArguments argnfo_RF = {};
            if (evln.tag == Tag::RenameFrom) {
                auto result = StreamParser::parse_rename_short_arguments(evln);
                if (result.is_error())
                    return result;
                argnfo_RF = result.value();
            } else {
                auto result = StreamParser::parse_rename2_short_arguments(evln);
                if (result.is_error())
                    return result;
                argnfo_RF = result.value();
            }
            /* Get rename arguments */
            ++it;
            bound_check_iter(it, end_it);

            auto last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::RenameFromPath, Tag::RenameFromPathExtended, Tag::RenameFromPathEnd, from_path);

            if (size != argnfo_RF.fnamesize)
                return SizeMismatchError(last_it->line_number, size, argnfo_RF.fnamesize);

            bound_check_iter(it, end_it);

            auto &ievln = *it;
            if (ievln.tag == Tag::RenameTo) {
                RenameArguments argnfo_RT = {};
                auto result = StreamParser::parse_rename_short_arguments(ievln);
                if (result.is_error())
                    return result;

                argnfo_RT = result.value();
                ++it;
                bound_check_iter(it, end_it);

                last_it = it;
                size = StreamParser::parse_long_argument(it, end_it, Tag::RenameToPath, Tag::RenameToPathExtended, Tag::RenameToPathEnd, to_path);

                if (size != argnfo_RT.fnamesize)
                    return SizeMismatchError(last_it->line_number, argnfo_RT.fnamesize, size);

                Execution &e = process.executions.back();

                from.mode = O_RDONLY;
                from.absolute_path = from_path;
                to.mode = O_WRONLY;
                to.absolute_path = to_path;

                e.add_open_file(from_path, from);
                e.add_open_file(to_path, to);

                n_processed += 2;
                _stats_collector.increment_rename();
            } else if (ievln.tag == Tag::RenameFailed) {
                it++;
            } else {
                return {};
                break;
            }
        } break;
        case Tag::RenameFailed: {
            /* Sometimes it's just happening */
            it++;
        } break;
        case Tag::LinkFailed: {
            /* Didn't happen but eventually it will */
            it++;
        } break;
        case Tag::LinkFrom:
        case Tag::LinkatFrom: {
            std::string from_path;
            std::string to_path;
            auto from = OpenFile();
            auto to = OpenFile();
            LinkArguments argnfo_LF = {};
            if (evln.tag == Tag::LinkFrom) {
                auto result = StreamParser::parse_link_short_arguments(evln);
                if (result.is_error())
                    return result;

                argnfo_LF = result.value();
            } else {
                auto result = StreamParser::parse_linkat_short_arguments(evln);
                if (result.is_error())
                    return result;
                argnfo_LF = result.value();
            }
            /* Get link arguments */
            increment_bound_check_iter(it, end_it);

            auto last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::LinkFromPath, Tag::LinkFromPathExtended, Tag::LinkFromPathEnd, from_path);

            if (size != argnfo_LF.fnamesize)
                return SizeMismatchError(last_it->line_number, size, argnfo_LF.fnamesize);

            bound_check_iter(it, end_it);

            auto &ievln = *it;
            if (ievln.tag == Tag::LinkTo) {
                LinkArguments argnfo_LT = {};
                auto result = StreamParser::parse_link_short_arguments(ievln);
                if (result.is_error())
                    return result;

                argnfo_LT = result.value();
                increment_bound_check_iter(it, end_it);

                last_it = it;
                size = StreamParser::parse_long_argument(it, end_it, Tag::LinkToPath, Tag::LinkToPathExtended, Tag::LinkToPathEnd, to_path);

                if (size != argnfo_LT.fnamesize)
                    return SizeMismatchError(last_it->line_number, size, argnfo_LT.fnamesize);

                Execution &e = process.executions.back();

                to.mode = O_WRONLY;
                to.absolute_path = to_path;
                from.mode = O_RDONLY;
                from.absolute_path = to_path;

                e.add_open_file(to_path, to);
                e.add_open_file(from_path, from);

                n_processed += 2;
                _stats_collector.increment_link();
            } else if (ievln.tag == Tag::LinkFailed) {
                it++;
            } else {
                return {};
                break;
            }
        } break;
        case Tag::Symlink: {
            std::string original_target;
            std::string absolute_target;
            std::string absolute_symlink;
            auto from = OpenFile();
            auto to = OpenFile();
            SymlinkArguments argnfo = {};
            auto result = StreamParser::parse_symlink_short_arguments(evln);
            if (result.is_error())
                return result;

            argnfo = result.value();
            ++it;
            bound_check_iter(it, end_it);

            auto last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::SymlinkTargetName, Tag::SymlinkTargetNameExtended, Tag::SymlinkTargetNameEnd, original_target);

            bound_check_iter(it, end_it);

            if (size != argnfo.targetnamesize)
                return SizeMismatchError(last_it->line_number, size, argnfo.targetnamesize);

            if (argnfo.resolvednamesize != -1) {
                last_it = it;
                ssize_t size = StreamParser::parse_long_argument(it, end_it, Tag::SymlinkTargetPath, Tag::SymlinkTargetPathExtended, Tag::SymlinkTargetPathEnd, absolute_target);

                bound_check_iter(it, end_it);

                if (size != argnfo.resolvednamesize)
                    return SizeMismatchError(last_it->line_number, size, argnfo.resolvednamesize);
            }

            last_it = it;
            size = StreamParser::parse_long_argument(it, end_it, Tag::SymlinkPath, Tag::SymlinkPathExtended, Tag::SymlinkPathEnd, absolute_symlink);

            if (size != argnfo.linknamesize)
                return SizeMismatchError(last_it->line_number, size, argnfo.linknamesize);

            Execution &e = process.executions.back();

            to.mode = O_WRONLY;
            to.absolute_path = absolute_symlink;

            if (!absolute_target.empty()) {
                from.mode = O_RDONLY;
                from.absolute_path = absolute_target;

                e.add_open_file(absolute_target, from);
            }

            e.add_open_file(absolute_symlink, to);
            n_processed += 2;
            _stats_collector.increment_symlink();
        } break;
        case Tag::Exit: {
            if (first_event_time() == 0)
                set_first_event_time(evln.timestamp);
            auto result = parse_exit_short_arguments(evln);
            if (result.is_error())
                return result;
            auto args = result.value();
            Execution &e = process.executions.back();
            e.exit_code = args.status;
            n_processed++;
            _stats_collector.increment_exit();
            append_syscall(syscall_raw(process.pid, syscall_raw::SYS_EXIT, evln.timestamp - first_event_time()));
            it++;
        } break;
        default: {
            /* Try to ignore it */
            return {};
            it++;
            break;
        }
        }
    }


    auto &last_execution = process.executions.back();
    last_execution.elapsed_time = process.last_event_time - exe_start_time;

    process.event_list.clear();
    process.event_list.shrink_to_fit();

    _stats_collector.add_processed(n_processed);
    _stats_collector.add_execs(n_execs);
    _stats_collector.increment_processes();
    // return n_processed;

// error:
//     /* Pretend as if this process exited immediately after the fork */
//     this->ve.resize(ve_init_size + 1);
//     this->ve.back().settime(process.last_event_time - exe_start_time);
//     this->stats.total_event_count += n_processed;
//     this->stats.process_count++;
//     for (size_t u = 0; u < srvec.size() - srvec_init_size; ++u)
//         srvec.pop_back();
//     this->stats.events.exit_count++;
//     srvec.push_back(syscall_raw(process.pid, syscall_raw::SYS_EXIT, evnode.last_event_time - this->start_time));
//     return n_processed;
//
    return {};
}

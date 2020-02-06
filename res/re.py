from portcullis._re import StreamRegexpDatabase, RegexpDef, StreamRegexpMatcher
from portcullis._re import CASELESS, DOTALL, MULTILINE, SINGLEMATCH, ALLOWEMPTY, UTF8, UCP, PREFILTER, SOM_LEFTMOST
from portcullis._re import MODE_SOM_HORIZON_SMALL, MODE_SOM_HORIZON_MEDIUM, MODE_SOM_HORIZON_LARGE


def compile_stream(*args, report_leftmost=False, mode="medium"):
    mode = mode.lower()
    if mode == "small":
        mode = MODE_SOM_HORIZON_SMALL
    elif mode == "medium":
        mode = MODE_SOM_HORIZON_MEDIUM
    elif mode == "large":
        mode = MODE_SOM_HORIZON_LARGE
    else:
        raise ValueError(f"unknown mode '{mode}'")

    regexps = []
    som_used = report_leftmost
    for r in args:
        if isinstance(r, tuple):
            if len(r) != 2:
                raise ValueError(f"unexpected tuple: {r}")
            regexp, flags = r
        elif isinstance(r, str):
            regexp, flags = r, 0
        else:
            raise ValueError(f"unexpected argument of type '{r.__class__.__name__}'")

        if flags & SOM_LEFTMOST:
            som_used = True

        if report_leftmost:
            flags |= SOM_LEFTMOST

        regexps.append(RegexpDef(regexp, flags))

    if not report_leftmost and not som_used:
        mode = 0

    return StreamRegexpDatabase(regexps, mode=mode)

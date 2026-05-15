#!/usr/bin/env python3

import json
from pathlib import Path


INPUT_JSON = "app_proto.json"

OUT_C = "app_proto_gen.h"
OUT_JS = "appproto_gen.js"


# ---------------------------------------------------------
# Helpers
# ---------------------------------------------------------

def emit_define(name, value):
    return f'#define {name:<24} "{value}"'


def emit_js_const(name, value):
    return f'const {name} = "{value}";'


def emit_comment(txt):
    return f"/* {txt} */"


# ---------------------------------------------------------
# Load JSON
# ---------------------------------------------------------

with open(INPUT_JSON, "r", encoding="utf-8") as f:
    proto = json.load(f)


# ---------------------------------------------------------
# Generate C header
# ---------------------------------------------------------

c_lines = []

c_lines.append("/*")
c_lines.append(" * AUTO-GENERATED FILE")
c_lines.append(" * DO NOT EDIT MANUALLY")
c_lines.append(" */")
c_lines.append("")

c_lines.append("#ifndef APP_PROTO_GEN_H_")
c_lines.append("#define APP_PROTO_GEN_H_")
c_lines.append("")

# protocol section
p = proto["protocol"]

c_lines.append(emit_comment("Protocol"))
c_lines.append(f'#define PROTO_VERSION         {p["version"]}')
c_lines.append(f"#define SEP                   '\\1'")
c_lines.append(f'#define MAX_PARAMS            {p["max_params"]}')
c_lines.append(f'#define MAX_TOKENS            {p["max_tokens"]}')
c_lines.append("")


# generic section emitter
def emit_group_c(group_name, group):
    c_lines.append(emit_comment(group_name))
    for k, v in group.items():
        c_lines.append(emit_define(k, v["value"]))
    c_lines.append("")


emit_group_c("Parameters", proto["parameters"])
emit_group_c("Operations", proto["operations"])
emit_group_c("Commands", proto["commands"])
emit_group_c("Responses", proto["responses"])
emit_group_c("URC", proto["urc"])

c_lines.append("#endif")
c_lines.append("")


# ---------------------------------------------------------
# Generate JS
# ---------------------------------------------------------

js_lines = []

js_lines.append("// AUTO-GENERATED FILE")
js_lines.append("// DO NOT EDIT MANUALLY")
js_lines.append("")


js_lines.append("// Protocol")
js_lines.append(f'const PROTO_VERSION = {p["version"]};')
js_lines.append(r'const SEP = "\x01";')
js_lines.append(f'const MAX_PARAMS = {p["max_params"]};')
js_lines.append(f'const MAX_TOKENS = {p["max_tokens"]};')
js_lines.append("")


def emit_group_js(group_name, group):
    js_lines.append(f"// {group_name}")
    for k, v in group.items():
        js_lines.append(emit_js_const(k, v["value"]))
    js_lines.append("")


emit_group_js("Parameters", proto["parameters"])
emit_group_js("Operations", proto["operations"])
emit_group_js("Commands", proto["commands"])
emit_group_js("Responses", proto["responses"])
emit_group_js("URC", proto["urc"])


# ---------------------------------------------------------
# Write files
# ---------------------------------------------------------

Path(OUT_C).write_text(
    "\n".join(c_lines),
    encoding="utf-8"
)

Path(OUT_JS).write_text(
    "\n".join(js_lines),
    encoding="utf-8"
)

print(f"Generated:")
print(f"  {OUT_C}")
print(f"  {OUT_JS}")
#!/usr/bin/env python3

import json
import sys
from pathlib import Path


# ---------------------------------------------------------
# Helpers
# ---------------------------------------------------------

def emit_define(name, value):
    return f'#define {name:<28} "{value}"'


def emit_js_const(name, value):
    return f'const {name} = "{value}";'


def emit_comment(txt):
    return f"/* {txt} */"


# ---------------------------------------------------------
# Check arguments
# ---------------------------------------------------------

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <proto.json>")
    sys.exit(1)

json_path = Path(sys.argv[1])

if not json_path.exists():
    print(f"JSON file not found: {json_path}")
    sys.exit(1)

base = json_path.stem

out_c = json_path.with_suffix(".h")
out_js = json_path.with_suffix(".js")
out_md = json_path.with_suffix(".md")


# ---------------------------------------------------------
# Load JSON
# ---------------------------------------------------------

with open(json_path, "r", encoding="utf-8") as f:
    proto = json.load(f)


# ---------------------------------------------------------
# Generate C header
# ---------------------------------------------------------

guard = f"{base.upper()}_H_".replace("-", "_")

c_lines = []

c_lines.append("/*")
c_lines.append(" * AUTO-GENERATED FILE")
c_lines.append(" * DO NOT EDIT MANUALLY")
c_lines.append(" */")
c_lines.append("")

c_lines.append(f"#ifndef {guard}")
c_lines.append(f"#define {guard}")
c_lines.append("")

p = proto["protocol"]

c_lines.append(emit_comment(p.get("comment", "Protocol")))
c_lines.append(f'#define PROTO_VERSION             {p["version"]}')
c_lines.append(f"#define SEP                       '\\1'")
c_lines.append(f'#define MAX_PARAMS                {p["max_params"]}')
c_lines.append(f'#define MAX_TOKENS                {p["max_tokens"]}')
c_lines.append("")

SECTIONS = [
    "parameters",
    "operations",
    "commands",
    "responses",
    "URCs"
]
def emit_group_c(group_name, group):
    c_lines.append(emit_comment(group_name))

    for k, v in group.items():

        if "comment" in v:
            comment = v["comment"]
            # multiline comment
            if isinstance(comment, list):
                c_lines.append("/*")
                for line in comment:
                    c_lines.append(f" * {line}")
                c_lines.append(" */")
            # single line comment
            else:
                c_lines.append(f"/* {comment} */")

        c_lines.append(emit_define(k, v["value"]))
        c_lines.append("")


for sec in SECTIONS:
    if sec in proto:
        emit_group_c(sec.capitalize(), proto[sec])

c_lines.append(f"#endif /* {guard} */")
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
        if "comment" in v:
            comment = v["comment"]
            # multiline comment
            if isinstance(comment, list):
                js_lines.append("/*")
                for line in comment:
                    js_lines.append(f" * {line}")
                js_lines.append(" */")
            # single line comment
            else:
                js_lines.append(f"/* {comment} */")

        js_lines.append(emit_js_const(k, v["value"]))
        js_lines.append("")


for sec in SECTIONS:
    if sec in proto:
        emit_group_js(sec.capitalize(), proto[sec])


# ---------------------------------------------------------
# Generate Markdown documentation
# ---------------------------------------------------------

md = []

md.append(f"# {p.get('name', base)}")
md.append("")
md.append(p.get("comment", ""))
md.append("file automatically generated. !!!do not edit!!!")
md.append("")

md.append("## Protocol")
md.append("")
md.append(f"- Version: `{p['version']}`")
md.append(f"- Separator: `0x01`")
md.append(f"- Max params: `{p['max_params']}`")
md.append(f"- Max tokens: `{p['max_tokens']}`")
md.append("")

md.append("## Format")
md.append("")
md.append("`[header]0x01[payload]`")
md.append("")
md.append("#### Header format")
md.append("`[ver]0x01[hdr_fields]0x01[payload_len]0x01[timestamp]0x01[cmd|response]0x01[param_0]0x01[param_1]0x01...0x01[param_n]0x01`")


def emit_md_section(title, section):

    md.append(f"## {title}")
    md.append("")

    for k, v in section.items():

        md.append(f"### {k}")
        md.append("")

        md.append(f"- Value: `{v['value']}`")

        if "comment" in v:
            comment = v["comment"]
            if isinstance(comment, list):
                md.append("- Description:")
                for line in comment:
                    md.append(f"  - {line}")
            else:
                md.append(f"- Description: {comment}")

        if "hdr_fields" in v:
            md.append(f"- no of header fields: {v['hdr_fields']}")
        if "payload" in v:
            md.append(f"- Payload: `{v['payload']}`")

        if "payload_type" in v:
            md.append(f"- Payload type: `{v['payload_type']}`")

        if "params" in v and len(v["params"]) > 0:

            md.append("")
            md.append("| Parameter | Type | Description |")
            md.append("|---|---|---|")

            for p in v["params"]:

                if isinstance(p, str):
                    md.append(f"| {p} | - | - |")
                else:
                    md.append(
                        f"| {p.get('name','')} | "
                        f"{p.get('type','')} | "
                        f"{p.get('comment','')} |"
                    )

        md.append("")


for sec in SECTIONS:

    if sec in proto:
        emit_md_section(sec.capitalize(), proto[sec])


# ---------------------------------------------------------
# Write files
# ---------------------------------------------------------

out_c.write_text("\n".join(c_lines), encoding="utf-8")
out_js.write_text("\n".join(js_lines), encoding="utf-8")
out_md.write_text("\n".join(md), encoding="utf-8")

print("Generated:")
print(f"  {out_c}")
print(f"  {out_js}")
print(f"  {out_md}")
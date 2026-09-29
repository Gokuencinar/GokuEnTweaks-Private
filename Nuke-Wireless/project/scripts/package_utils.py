"""Build the Nuke Wireless RootHide package from its user-supplied base app.

The original maintainer scripts are treated as data and are never executed.
"""

from __future__ import annotations

from dataclasses import dataclass
import io
import os
from pathlib import Path, PurePosixPath
import plistlib
import tarfile


SOURCE_PATH = os.environ.get("NUKE_WIRELESS_SOURCE_DEB")
if not SOURCE_PATH:
    raise KeyError("Set NUKE_WIRELESS_SOURCE_DEB to the original package path")
SOURCE = Path(SOURCE_PATH)


@dataclass(frozen=True)
class SourceLayout:
    app_root: str
    info_plist: str
    executable: str
    bundle_id: str
    helper_root: str
    aegis: str
    arp_scan: str
    arpspoof: str


def normalize_payload_path(name: str) -> str:
    name = name.lstrip("./")
    if name.startswith("var/jb/"):
        name = name[len("var/jb/"):]
    return name


def discover_source_layout(data_tar: bytes) -> SourceLayout:
    files: set[str] = set()
    info_candidates: list[tuple[str, bytes]] = []
    with tarfile.open(fileobj=io.BytesIO(data_tar), mode="r:*") as tf:
        for item in tf:
            name = normalize_payload_path(item.name)
            if not name:
                continue
            if item.isfile():
                files.add(name)
            path = PurePosixPath(name)
            if (
                item.isfile()
                and path.name == "Info.plist"
                and path.parent.suffix == ".app"
                and path.parent.parent.as_posix() == "Applications"
            ):
                info_candidates.append((name, tf.extractfile(item).read()))
    if len(info_candidates) != 1:
        raise ValueError(f"expected exactly one app Info.plist; found {len(info_candidates)}")

    info_path, info_data = info_candidates[0]
    info = plistlib.loads(info_data)
    executable_name = info.get("CFBundleExecutable")
    bundle_id = info.get("CFBundleIdentifier")
    if not isinstance(executable_name, str) or not executable_name:
        raise ValueError("source app has no CFBundleExecutable")
    if not isinstance(bundle_id, str) or not bundle_id:
        raise ValueError("source app has no CFBundleIdentifier")
    app_root = PurePosixPath(info_path).parent.as_posix()
    executable = f"{app_root}/{executable_name}"
    if executable not in files:
        raise ValueError(f"missing source app executable: {executable}")

    helper_roots = []
    for name in files:
        path = PurePosixPath(name)
        if path.name != "aegis" or not name.startswith("usr/libexec/"):
            continue
        root = path.parent.as_posix()
        if f"{root}/arp-scan" in files and f"{root}/arpspoof" in files:
            helper_roots.append(root)
    if len(helper_roots) != 1:
        raise ValueError(f"expected exactly one helper directory; found {len(helper_roots)}")
    helper_root = helper_roots[0]
    return SourceLayout(
        app_root=app_root,
        info_plist=info_path,
        executable=executable,
        bundle_id=bundle_id,
        helper_root=helper_root,
        aegis=f"{helper_root}/aegis",
        arp_scan=f"{helper_root}/arp-scan",
        arpspoof=f"{helper_root}/arpspoof",
    )


def extract_payload_file(data_tar: bytes, normalized_path: str) -> bytes:
    with tarfile.open(fileobj=io.BytesIO(data_tar), mode="r:*") as tf:
        for item in tf:
            if normalize_payload_path(item.name) != normalized_path:
                continue
            if not item.isfile():
                raise ValueError(f"payload path is not a file: {normalized_path}")
            return tf.extractfile(item).read()
    raise ValueError(f"payload file not found: {normalized_path}")



def read_ar(data: bytes) -> dict[str, bytes]:
    if not data.startswith(b"!<arch>\n"):
        raise ValueError("not an ar archive")
    parts: dict[str, bytes] = {}
    offset = 8
    while offset < len(data):
        header = data[offset : offset + 60]
        if len(header) != 60 or header[58:60] != b"`\n":
            raise ValueError("invalid ar header")
        name = header[:16].decode("ascii").strip().rstrip("/")
        size = int(header[48:58].decode("ascii").strip())
        parts[name] = data[offset + 60 : offset + 60 + size]
        offset += 60 + size + size % 2
    return parts


def pack_ar(parts: list[tuple[str, bytes]]) -> bytes:
    out = bytearray(b"!<arch>\n")
    for name, data in parts:
        if len(name) > 15:
            raise ValueError("ar member name too long")
        header = (
            f"{name + '/':<16}{0:<12}{0:<6}{0:<6}{format(0o100644, 'o'):<8}{len(data):<10}`\n"
        ).encode("ascii")
        assert len(header) == 60
        out.extend(header)
        out.extend(data)
        if len(data) % 2:
            out.extend(b"\n")
    return bytes(out)


def tar_bytes(entries: list[tuple[tarfile.TarInfo, bytes | None]]) -> bytes:
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode="w:gz", format=tarfile.GNU_FORMAT) as tf:
        for info, data in entries:
            info.uid = 0
            info.gid = 0
            info.uname = "root"
            info.gname = "wheel"
            tf.addfile(info, io.BytesIO(data) if data is not None else None)
    return stream.getvalue()


def regular(path: str, data: bytes, mode: int = 0o644) -> tuple[tarfile.TarInfo, bytes]:
    info = tarfile.TarInfo("./" + path)
    info.mode = mode
    info.size = len(data)
    return info, data


def symlink(path: str, target: str) -> tuple[tarfile.TarInfo, None]:
    info = tarfile.TarInfo("./" + path)
    info.type = tarfile.SYMTYPE
    info.mode = 0o777
    info.linkname = target
    return info, None


def get_tar_member(parts: dict[str, bytes], prefix: str) -> bytes:
    found = [(name, data) for name, data in parts.items() if name.startswith(prefix)]
    if len(found) != 1:
        raise ValueError(f"expected exactly one {prefix} member")
    return found[0][1]





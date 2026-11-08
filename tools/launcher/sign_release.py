#!/usr/bin/env python3
"""Signs LTCallisto's release packages, which the launcher's updater refuses to install unsigned.

  sign_release.py keygen <secret key file>
      Makes the key pair once: the secret goes in the file given, which must be outside the repository,
      and the public half goes in update_key.txt beside this script, which is committed.
  sign_release.py sign <secret key file> <package>...
      Writes <package>.sig beside each package. Upload it to the release with its package.
  sign_release.py verify <package>...
      Checks each package and its .sig against update_key.txt, as the launcher does.
  sign_release.py check <secret key file>
      Checks that the secret key is the one update_key.txt holds, without signing anything.

tools/build_release.bat builds both packages and signs them with this.

A .sig signs the package's file name and SHA-256 together, so a genuine older package can't be
offered under a newer release's name. Ed25519 (RFC 8032), with only Python's standard library.
"""
import datetime
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
PUBLIC_KEY_FILE = os.path.join(HERE, "update_key.txt")

# The launcher builds the same text (CheckReleaseSignature in launcher_update.cpp)
SIGNED_TEXT = "LTCallisto release signature 1\nfile={file}\nsha256={sha256}\n"

# Ed25519 after RFC 8032's reference code (section 6). Signing runs on the release maker's own machine,
# so its timing doesn't matter
p = 2**255 - 19
q = 2**252 + 27742317777372353535851937790883648493
d = -121665 * pow(121666, p - 2, p) % p
SQRT_M1 = pow(2, (p - 1) // 4, p)


def sha512_modq(data):
    return int.from_bytes(hashlib.sha512(data).digest(), "little") % q


def point_add(P, Q):
    A = (P[1] - P[0]) * (Q[1] - Q[0]) % p
    B = (P[1] + P[0]) * (Q[1] + Q[0]) % p
    C = 2 * P[3] * Q[3] * d % p
    D = 2 * P[2] * Q[2] % p
    E, F, G, H = B - A, D - C, D + C, B + A
    return (E * F % p, G * H % p, F * G % p, E * H % p)


def point_mul(s, P):
    Q = (0, 1, 1, 0)
    while s > 0:
        if s & 1:
            Q = point_add(Q, P)
        P = point_add(P, P)
        s >>= 1
    return Q


def point_equal(P, Q):
    return (P[0] * Q[2] - Q[0] * P[2]) % p == 0 and (P[1] * Q[2] - Q[1] * P[2]) % p == 0


def recover_x(y, sign):
    if y >= p:
        return None
    x2 = (y * y - 1) * pow(d * y * y + 1, p - 2, p) % p
    if x2 == 0:
        return None if sign else 0
    x = pow(x2, (p + 3) // 8, p)
    if (x * x - x2) % p:
        x = x * SQRT_M1 % p
    if (x * x - x2) % p:
        return None
    if (x & 1) != sign:
        x = p - x
    return x


G_Y = 4 * pow(5, p - 2, p) % p
G_X = recover_x(G_Y, 0)
G = (G_X, G_Y, 1, G_X * G_Y % p)


def point_compress(P):
    zinv = pow(P[2], p - 2, p)
    x, y = P[0] * zinv % p, P[1] * zinv % p
    return int.to_bytes(y | ((x & 1) << 255), 32, "little")


def point_decompress(s):
    y = int.from_bytes(s, "little")
    sign = y >> 255
    y &= (1 << 255) - 1
    x = recover_x(y, sign)
    return None if x is None else (x, y, 1, x * y % p)


def secret_expand(secret):
    h = hashlib.sha512(secret).digest()
    a = int.from_bytes(h[:32], "little")
    a &= (1 << 254) - 8
    a |= 1 << 254
    return a, h[32:]


def public_key(secret):
    return point_compress(point_mul(secret_expand(secret)[0], G))


def sign(secret, msg):
    a, prefix = secret_expand(secret)
    A = point_compress(point_mul(a, G))
    r = sha512_modq(prefix + msg)
    R = point_compress(point_mul(r, G))
    s = (r + sha512_modq(R + A + msg) * a) % q
    return R + int.to_bytes(s, 32, "little")


def verify(public, msg, signature):
    A = point_decompress(public)
    R = point_decompress(signature[:32])
    s = int.from_bytes(signature[32:], "little")
    if A is None or R is None or s >= q:
        return False
    h = sha512_modq(signature[:32] + public + msg)
    return point_equal(point_mul(s, G), point_add(R, point_mul(h, A)))


def read_hex_line(path, nbytes, what):
    """The first line of a file that is nbytes of hex, skipping # comments"""
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                if len(line) == 2 * nbytes:
                    try:
                        return bytes.fromhex(line)
                    except ValueError:
                        pass
                break
    sys.exit("%s holds no %s" % (path, what))


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_sig(path):
    fields = {}
    with open(path, encoding="utf-8", newline="") as f:
        for line in f.read().split("\n"):
            line = line.rstrip("\r")
            if "=" in line:
                k, v = line.split("=", 1)
                fields[k] = v
    return fields


def cmd_keygen(secret_path):
    secret_path = os.path.abspath(secret_path)
    if os.path.commonpath([secret_path, REPO]) == REPO:
        sys.exit("Keep the secret key outside the repository, so it is never committed")
    if os.path.exists(secret_path):
        sys.exit("%s already exists. Remove it first if you mean to replace the key" % secret_path)
    secret = os.urandom(32)
    public = public_key(secret)
    os.makedirs(os.path.dirname(secret_path), exist_ok=True)
    fd = os.open(secret_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as f:
        f.write("# LTCallisto's release signing key. Secret: back it up and never commit or share it\n")
        f.write(secret.hex() + "\n")
    with open(PUBLIC_KEY_FILE, "w", encoding="utf-8", newline="\n") as f:
        f.write("# The key LTCallisto's updater checks releases against (sign_release.py), made %s\n"
                % datetime.date.today().isoformat())
        f.write(public.hex() + "\n")
    print("secret key: %s" % secret_path)
    print("public key: %s (commit this)" % PUBLIC_KEY_FILE)
    print("            %s" % public.hex())


def load_secret(secret_path):
    """The secret key, once it's known to match update_key.txt"""
    if not os.path.isfile(secret_path):
        sys.exit("There is no secret key at %s" % secret_path)
    secret = read_hex_line(secret_path, 32, "secret key")
    if public_key(secret) != read_hex_line(PUBLIC_KEY_FILE, 32, "public key"):
        sys.exit("%s is not the key in update_key.txt, so the launcher would refuse what it signs" % secret_path)
    return secret


def cmd_check(secret_path):
    load_secret(secret_path)
    print("%s matches update_key.txt" % secret_path)


def cmd_sign(secret_path, packages):
    secret = load_secret(secret_path)
    for package in packages:
        name = os.path.basename(package)
        digest = sha256_file(package)
        signature = sign(secret, SIGNED_TEXT.format(file=name, sha256=digest).encode("ascii"))
        with open(package + ".sig", "w", encoding="ascii", newline="\n") as f:
            f.write("file=%s\nsha256=%s\ned25519=%s\n" % (name, digest, signature.hex()))
        print("signed %s -> %s.sig" % (name, name))
        cmd_verify([package])


def cmd_verify(packages):
    public = read_hex_line(PUBLIC_KEY_FILE, 32, "public key")
    bad = 0
    for package in packages:
        name = os.path.basename(package)
        fields = parse_sig(package + ".sig")
        try:
            signature = bytes.fromhex(fields.get("ed25519", ""))
        except ValueError:
            signature = b""
        problem = None
        if fields.get("file") != name:
            problem = "its .sig names %s" % fields.get("file")
        elif fields.get("sha256") != sha256_file(package):
            problem = "it isn't the file its .sig was made for"
        elif len(signature) != 64 or not verify(
                public, SIGNED_TEXT.format(file=name, sha256=fields["sha256"]).encode("ascii"), signature):
            problem = "its signature doesn't match update_key.txt"
        print("%s %s%s" % ("ok  " if not problem else "BAD ", name, ": " + problem if problem else ""))
        bad += problem is not None
    if bad:
        sys.exit(1)


def main(argv):
    if len(argv) == 2 and argv[0] == "keygen":
        cmd_keygen(argv[1])
    elif len(argv) >= 3 and argv[0] == "sign":
        cmd_sign(argv[1], argv[2:])
    elif len(argv) >= 2 and argv[0] == "verify":
        cmd_verify(argv[1:])
    elif len(argv) == 2 and argv[0] == "check":
        cmd_check(argv[1])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])

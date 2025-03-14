# to test arbitrary precision integers

x = 1000000000000000000000000000000
xn = -1000000000000000000000000000000
y = 2000000000000000000000000000000

# printing
print(x)
print(y)
print("%#X" % (x - x))  # print prefix
print("{:#,}".format(x))  # print with commas

# construction
print(int(x))

# addition
print(x + 1)
print(x + y)
print(x + xn == 0)
print(bool(x + xn))

# subtraction
print(x - 1)
print(x - y)
print(y - x)
print(x - x == 0)
print(bool(x - x))

# multiplication
print(x * 2)
print(x * y)

# integer division
print(x // 2)
print(y // x)

# bit inversion
print(~x)
print(~(-x))

# left shift
x = 0x10000000000000000000000
for i in range(32):
    x = x << 1
    print(x)

# right shift
x = 0x10000000000000000000000
for i in range(32):
    x = x >> 1
    print(x)

# left shift of a negative number
for i in range(8):
    print(-10000000000000000000000000 << i)
    print(-10000000000000000000000001 << i)
    print(-10000000000000000000000002 << i)
    print(-10000000000000000000000003 << i)
    print(-10000000000000000000000004 << i)

# right shift of a negative number
for i in range(8):
    print(-10000000000000000000000000 >> i)
    print(-10000000000000000000000001 >> i)
    print(-10000000000000000000000002 >> i)
    print(-10000000000000000000000003 >> i)
    print(-10000000000000000000000004 >> i)

# conversion from string
print(int("123456789012345678901234567890"))
print(int("-123456789012345678901234567890"))
print(int("123456789012345678901234567890abcdef", 16))
print(int("123456789012345678901234567890ABCDEF", 16))
print(int("1234567890abcdefghijklmnopqrstuvwxyz", 36))

# invalid characters in string
try:
    print(int("123456789012345678901234567890abcdef"))
except ValueError:
    print("ValueError")
try:
    print(int("123456789012345678901234567890\x01"))
except ValueError:
    print("ValueError")

# test constant integer with more than 255 chars
x = 0x84CE72AA8699DF436059F052AC51B6398D2511E49631BCB7E71F89C499B9EE425DFBC13A5F6D408471B054F2655617CBBAF7937B7C80CD8865CF02C8487D30D2B0FBD8B2C4E102E16D828374BBC47B93852F212D5043C3EA720F086178FF798CC4F63F787B9C2E419EFA033E7644EA7936F54462DC21A6C4580725F7F0E7D1AAAAAAA
print(x)

# test parsing ints just on threshold of small to big
# for 32 bit archs
x = 1073741823  # small
x = -1073741823  # small
x = 1073741824  # big
x = -1073741824  # big
# for nan-boxing with 47-bit small ints
print(int("0x3fffffffffff", 16))  # small
print(int("-0x3fffffffffff", 16))  # small
print(int("0x400000000000", 16))  # big
print(int("-0x400000000000", 16))  # big
# for 64 bit archs
x = 4611686018427387903  # small
x = -4611686018427387903  # small
x = 4611686018427387904  # big
x = -4611686018427387904  # big

# sys.maxsize is a constant mpz, so test it's compatible with dynamic ones
import sys

print(sys.maxsize + 1 - 1 == sys.maxsize)

# test extraction of big int value via mp_obj_get_int_maybe
x = 1 << 70
print("a" * (x + 4 - x))

print(() == ())
print(() > ())
print(() < ())
print(() == (1,))
print((1,) == ())
print(() > (1,))
print((1,) > ())
print(() < (1,))
print((1,) < ())
print(() >= (1,))
print((1,) >= ())
print(() <= (1,))
print((1,) <= ())

print((1,) == (1,))
print((1,) != (1,))
print((1,) == (2,))
print(
    (1,)
    == (
        1,
        0,
    )
)

print((1,) > (1,))
print((1,) > (2,))
print((2,) > (1,))
print(
    (
        1,
        0,
    )
    > (1,)
)
print(
    (
        1,
        -1,
    )
    > (1,)
)
print(
    (1,)
    > (
        1,
        0,
    )
)
print(
    (1,)
    > (
        1,
        -1,
    )
)

print((1,) < (1,))
print((2,) < (1,))
print((1,) < (2,))
print(
    (1,)
    < (
        1,
        0,
    )
)
print(
    (1,)
    < (
        1,
        -1,
    )
)
print(
    (
        1,
        0,
    )
    < (1,)
)
print(
    (
        1,
        -1,
    )
    < (1,)
)

print((1,) >= (1,))
print((1,) >= (2,))
print((2,) >= (1,))
print(
    (
        1,
        0,
    )
    >= (1,)
)
print(
    (
        1,
        -1,
    )
    >= (1,)
)
print(
    (1,)
    >= (
        1,
        0,
    )
)
print(
    (1,)
    >= (
        1,
        -1,
    )
)

print((1,) <= (1,))
print((2,) <= (1,))
print((1,) <= (2,))
print(
    (1,)
    <= (
        1,
        0,
    )
)
print(
    (1,)
    <= (
        1,
        -1,
    )
)
print(
    (
        1,
        0,
    )
    <= (1,)
)
print(
    (
        1,
        -1,
    )
    <= (1,)
)

print((10, 0) > (1, 1))
print((10, 0) < (1, 1))
print((0, 0, 10, 0) > (0, 0, 1, 1))
print((0, 0, 10, 0) < (0, 0, 1, 1))


print(() == {})
print(() != {})
print((1,) == [1])

try:
    print(() < {})
except TypeError:
    print("TypeError")

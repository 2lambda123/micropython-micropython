print(repr(IndexError()))
print(str(IndexError()))

print(repr(IndexError("foo")))
print(str(IndexError("foo")))

a = IndexError(1, "test", [100, 200])
print(str(a))
print(repr(a))

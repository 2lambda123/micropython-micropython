# test builtin pow() with integral values
# 3 arg version

try:
    print(pow(3, 4, 7))
except NotImplementedError:
    print("SKIP")
    raise SystemExit

print(pow(555557, 1000002, 1000003))

# Tests for 3 arg pow with large values

# This value happens to be prime
x = 0xD48A1E2A099B1395895527112937A391D02D4A208BCE5D74B281CF35A57362502726F79A632F063A83C0EBA66196712D963AA7279AB8A504110A668C0FC38A7983C51E6EE7A85CAE87097686CCDC359EE4BBF2C583BCE524E3F7836BDED1C771A4EFCB25C09460A862FC98E18F7303DF46AAEB34DA46B0C4D61D5CD78350F3EDB60E6BC4BEFA712A849
y = 0x3ACCF60BB1A5365E4250D1588EB0FE6CD81AD495E9063F90880229F2A625E98C59387238670936AFB2CAFC5B79448E4414D6CD5E9901AA845AA122DB58DDD7B9F2B17414600A18C47494ED1F3D49D005A5

print(hex(pow(2, 200, x)))  # Should not overflow, just 1 << 200
print(hex(pow(2, x - 1, x)))  # Should be 1, since x is prime
print(hex(pow(y, x - 1, x)))  # Should be 1, since x is prime
print(hex(pow(y, y - 1, x)))  # Should be a 'big value'
print(hex(pow(y, y - 1, y)))  # Should be a 'big value'

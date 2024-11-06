# tests transition from small to large int representation by addition

# 31-bit overflow
i = 0x3FFFFFFF
print(i + i)
print(-i + -i)

# 47-bit overflow
i = 0x3FFFFFFFFFFF
print(i + i)
print(-i + -i)

# 63-bit overflow
i = 0x3FFFFFFFFFFFFFFF
print(i + i)
print(-i + -i)

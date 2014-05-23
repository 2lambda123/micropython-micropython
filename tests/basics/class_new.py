class A:

    @staticmethod
    def __new__(cls):
        print("A.__new__")
        return super(cls, A).__new__(cls)

    def __init__(self):
        pass

    def meth(self):
        pass

#print(A.__new__)
#print(A.__init__)

a = A()

#print(a.meth)
#print(a.__init__)
#print(a.__new__)

# Import all symbols from the .so module
from .my_python_module import alpha, zeta, Nested, ComplexWrapper, empty, beta

# Make these symbols available at the package level
globals().update({
    'alpha': alpha,
    'zeta': zeta,
    'Nested': Nested,
    'ComplexWrapper': ComplexWrapper,
    'empty': empty,
    'beta': beta
})

__all__ = [
    'alpha',
    'zeta',
    'Nested',
    'ComplexWrapper',
    'empty',
    'beta'
]
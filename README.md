# cppglue
customizable clang based tool to extract c++ code for input to language binding generators

## Build docker image
```bash
docker build -t llvm-runtime .
```

## Run docker image

Key components for secure container execution:

### User Permissions and Storage
- `--user $(id -u):$(id -g)` - Preserves host system permissions by running as current user
- `-v /tmp/llvm1:/app` - Maps local storage to container for persistent data

### Security Measures
- `--security-opt=no-new-privileges` - Prevents privilege escalation within container
- `--cap-drop=ALL` - Removes all Linux kernel capabilities, minimizing attack surface

### Interactive Access
- `-it` - Enables interactive terminal access (combines stdin and TTY allocation)

```bash
mkdir -p /tmp/llvm1
docker run -it --user $(id -u):$(id -g) -v /tmp/llvm1:/app --security-opt=no-new-privileges --cap-drop=ALL llvm-runtime --install /app
export PATH=$PATH:/tmp/llvm1/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/tmp/llvm1/lib

# You should now be be able to run clang and llvm tools
```


## Python Bindings
```bash
# Generate python bindings
# enter llvm-runtime container
# cmake .. -G Ninja -DCMAKE_INSTALL_PREFIX=/app
# ninja install
```

## Testing Python Package Installation

After building the Python bindings, you can test the package installation:

1. Create a virtual environment:
```bash
python -m venv venv
source venv/bin/activate
```

2. Install the package in development mode:
```bash
pip install -e .
```

3. Test importing the module:
```python
import cppglue
# Your module should now be available
```

4. To build a wheel for distribution:
```bash
python -m build
```

The wheel file will be created in the `dist/` directory.

This example is made using the work of the following project (https://github.com/TheBeef/BittyHTTP). I have added a simple file loader to parse an HTML file called index.html in the same directory as the output 
server binary. This file will have to be loaded into memory to be served, so don't make it too large.

As this server will only work with HTTP1.1, passing it through a reverse proxy like Nginx may require that you enforce a specific implementation of HTTP.

build with

```
docker run -it \
    -v $(pwd):/home/milkv/buildroot \
    ghcr.io/aidancrowther/milkvduocompile:latest \
    "cd examples/webserver && make"
```

# tcp-cat

The UNIX `cat` but for TCP connections.

## Build
```bash
$ bare-dev build
```

## Usage

```js
const tcpCat = require('.');

const ip = '127.0.0.1';
const port = 5050;
const request = `GET / HTTP/1.1\r\nHost: localhost:${port}\r\n\r\n`;

(async () => {
  try {
    const buffResponse = await tcpCat(ip, port, request);
    console.log(buffResponse.toString());
  } catch (err) {
    console.error(err);
  }
})();
```

## License

MIT

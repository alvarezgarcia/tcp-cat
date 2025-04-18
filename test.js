const test = require('brittle');

const tcpCat = require('.');

test('Invalid argument types', async (t) => {
  t.plan(1);

  const ip = '127.0.0.1';
  const port = '5050';
  const request = `GET / HTTP/1.1\r\nHost: localhost:${port}\r\n\r\n`;

  try {
    await tcpCat(ip, port, request);
  } catch (err) {
    t.is(err.code, 'ERR_INVALID_ARG_TYPE')
  }
});

test('Missing arguments', async (t) => {
  t.plan(1);

  const ip = '127.0.0.1';
  const port = 5050;

  try {
    await tcpCat(ip, port);
  } catch (err) {
    t.is(err.code, 'ERR_INVALID_ARG_TYPE')
  }
});

test('Refuse connection', async (t) => {
  t.plan(1);

  const ip = '127.0.0.1';
  const port = 1;
  const request = `GET / HTTP/1.1\r\nHost: localhost:${port}\r\n\r\n`;

  try {
    await tcpCat(ip, port, request);
  } catch (err) {
    t.is(err.code, 'ECONNREFUSED')
  }
});

const binding = require.addon()

const DEFAULT_BUFFER_LENGTH = 65536;

function typeError(code, message) {
  const error = new TypeError(message)
  error.code = code
  return error
}

const assertOnInvalidArgType = (value, expectedType) => {
  if (typeof value !== expectedType) {
    const m = `${value} must be a ${expectedType}. Received type ${typeof value} (${value})`;
    throw typeError(
      'ERR_INVALID_ARG_TYPE',
      m
    )
  }
};

const tcpCat = async (ip, port, request) => new Promise((resolve, reject) => {
  assertOnInvalidArgType(ip, 'string');
  assertOnInvalidArgType(port, 'number');
  assertOnInvalidArgType(request, 'string');

  const onRead = (err, readSize) => {
    if (err) {
      return reject(err);
    }

    copy = Buffer.from(buffer.subarray(0, readSize));
  };

  const onClose = () => {
    resolve(copy);
  };

  const ctx = {
    handler: null,
  };

  const buffer = Buffer.alloc(DEFAULT_BUFFER_LENGTH)
  let copy;

  ctx.handler = binding.cat(
    ctx,
    buffer,
    ip,
    port,
    request,
    onRead,
    onClose
  );
});

module.exports = tcpCat;

# cdp-node

## playwright

- 크롬 생성

```nodejs
// ./node/node_modules/playwright-core/lib/server/browserType.js
class BrowserType extends _instrumentation.SdkObject {
  ... 
  // 크롬 프로세스 실행 - 파이프 모드
  async _launchProcess(progress, options, isPersistent, browserLogsCollector, userDataDir) {
    var _options$args;
    const {
      ignoreDefaultArgs,
      ignoreAllDefaultArgs,
      args = [],
      executablePath = null,
      handleSIGINT = true,
      handleSIGTERM = true,
      handleSIGHUP = true
    } = options;
    ...
    let transport = undefined;
    let browserProcess = undefined;
    const {
      launchedProcess,
      gracefullyClose,
      kill
    } = await (0, _processLauncher.launchProcess)({
      command: executable,
      args: browserArguments,
      env: this._amendEnvironment(env, userDataDir, executable, browserArguments),
      handleSIGINT,
      handleSIGTERM,
      handleSIGHUP,
      log: message => {
        ...
      },
      stdio: 'pipe',
      tempDirectories,
      attemptToGracefullyClose: async () => {
        ...
      },
      onExit: (exitCode, signal) => {
        ...
      }
    });
    ...
    if (options.useWebSocket) {
      transport = await _transport.WebSocketTransport.connect(progress, wsEndpoint);
    } else {
      const stdio = launchedProcess.stdio;
      transport = new _pipeTransport.PipeTransport(stdio[3], stdio[4]);
    }
    return {
      browserProcess,
      artifactsDir,
      userDataDir,
      transport
    };
  }
}

./node_modules/playwright-core/lib/utils/processLauncher.js
// 크롬 프로세스 실행 - 파이프 모드
async function launchProcess(options) {
  ...
  const spawnedProcess = childProcess.spawn(options.command, options.args || [], spawnOptions);
  ...
  return {
    launchedProcess: spawnedProcess,
    gracefullyClose,
    kill: killAndWait
  };
}
```

```nodejs
{
  id: 16,
  method: "Emulation.setDeviceMetricsOverride",
  params: {
    mobile: false,
    width: 1280,
    height: 720,
    screenWidth: 1280,
    screenHeight: 720,
    deviceScaleFactor: 1,
    screenOrientation: {
      angle: 0,
      type: "landscapePrimary",
    },
    dontSetVisibleSize: undefined,
  },
  sessionId: "E47AB19684C5C472416A4CDAAA8644D0",
}
{
  id: 42,
  method: "Page.captureScreenshot",
  params: {
    format: "png",
    quality: undefined,
    clip: {
      x: 0,
      y: 0,
      width: 1340,
      height: 2911,
      scale: 1,
    },
    captureBeyondViewport: true,
  },
  sessionId: "E47AB19684C5C472416A4CDAAA8644D0",
}
```


- 메시지 보내기 / 받기
- 디폴트 사이즈 : 1280 * 720
```nodejs
// ./node/node_modules/playwright-core/lib/server/chromium/crConnection.js
class CRConnection extends _events.EventEmitter {
  ...
  // cdp 메시지 send
  _rawSend(sessionId, method, params) {
    const id = ++this._lastId;
    const message = {
      id,
      method,
      params
    };
    if (sessionId) message.sessionId = sessionId;
    this._protocolLogger('send', message);
    this._transport.send(message);
    return id;
  }

  // cdp 메시지 recv 
  async _onMessage(message) {
    this._protocolLogger('receive', message);
    if (message.id === kBrowserCloseMessageId) return;
    const session = this._sessions.get(message.sessionId || '');
    if (session) session._onMessage(message);
  }
  ...
}

// ./node_modules/playwright-core/lib/server/pipeTransport.js
class PipeTransport {
  ...
  // cdp 메시지 send
  send(message) {
      if (this._closed) throw new Error('Pipe has been closed');
      this._pipeWrite.write(JSON.stringify(message));
      this._pipeWrite.write('\0');
  }
  
  // cdp 메시지 recv 
  _dispatch(buffer) {
      let end = buffer.indexOf('\0');
      if (end === -1) {
        this._pendingBuffers.push(buffer);
        return;
      }
      this._pendingBuffers.push(buffer.slice(0, end));
      const message = Buffer.concat(this._pendingBuffers).toString();
      this._waitForNextTask(() => {
        if (this.onmessage) this.onmessage.call(null, JSON.parse(message));
      });
      let start = end + 1;
      end = buffer.indexOf('\0', start);
      while (end !== -1) {
        const message = buffer.toString(undefined, start, end);
        this._waitForNextTask(() => {
          if (this.onmessage) this.onmessage.call(null, JSON.parse(message));
        });
        start = end + 1;
        end = buffer.indexOf('\0', start);
      }
      this._pendingBuffers = [buffer.slice(start)];
  }
  ...
}

const express = require('express');
const http = require('http');
const path = require('path');
const { Server } = require('socket.io');
const { WebSocketServer, WebSocket } = require('ws');

// # Configuration
const PORT = process.env.PORT || 3000;
const MAX_PLAYERS = Number(process.env.MAX_PLAYERS || 3);
const CARD_SIZE = 5;
const MAX_BALL = 75;
const COUNTDOWN_MS = 6000;
const ALLOW_SOFTWARE_ONLY = process.env.ALLOW_SOFTWARE_ONLY === 'true';

// # Server Setup
const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: {
    origin: process.env.CORS_ORIGIN || '*',
    methods: ['GET', 'POST'],
  },
});
const hardwareWss = new WebSocketServer({ noServer: true });

// # HTTP Routes
app.use(express.static(path.join(__dirname, 'public')));
app.get('/healthz', (_req, res) => res.json({ ok: true, state: game.phase }));
app.get('/api/status', (_req, res) => res.json(publicState()));

// # Game State
const game = {
  phase: 'idle',
  roundId: null,
  adminId: null,
  players: new Map(),
  calledNumbers: [],
  startedAt: null,
  endedAt: null,
  winner: null,
  hardware: {
    connected: false,
    deviceId: null,
    lastSeen: null,
    socket: null,
  },
  timers: {
    countdown: null,
  },
};

// # Player And State Helpers
function cleanName(name) {
  return String(name || '')
    .trim()
    .replace(/[^\w .'-]/g, '')
    .slice(0, 14) || 'Player';
}

function publicState() {
  return {
    lobbyId: 'arcade',
    maxPlayers: MAX_PLAYERS,
    phase: game.phase,
    roundId: game.roundId,
    players: [...game.players.values()].map((player) => ({
      id: player.id,
      name: player.name,
      seat: player.seat,
      ready: player.ready,
      connected: player.connected,
      admin: player.id === game.adminId,
    })),
    adminId: game.adminId,
    calledCount: game.calledNumbers.length,
    startedAt: game.startedAt,
    endedAt: game.endedAt,
    winner: game.winner,
    hardware: {
      connected: game.hardware.connected,
      deviceId: game.hardware.deviceId,
      lastSeen: game.hardware.lastSeen,
    },
  };
}

function playerState(socketId) {
  const player = game.players.get(socketId);
  if (!player) return null;
  ensureMarkedIndexes(player);
  return {
    id: player.id,
    name: player.name,
    seat: player.seat,
    ready: player.ready,
    admin: player.id === game.adminId,
    card: player.card,
    markedIndexes: [...player.markedIndexes],
  };
}

function ensureMarkedIndexes(player) {
  if (!(player.markedIndexes instanceof Set)) {
    player.markedIndexes = new Set([12]);
  }
  player.markedIndexes.add(12);
}

function broadcastState() {
  io.emit('lobby_state', publicState());
  for (const [socketId] of game.players) {
    io.to(socketId).emit('player_state', playerState(socketId));
  }
}

function pruneDisconnectedLobbyPlayers() {
  if (game.phase === 'countdown' || game.phase === 'playing') return;
  for (const [socketId, player] of game.players) {
    if (!player.connected) game.players.delete(socketId);
  }
  assignAdmin();
  if (!game.players.size) {
    game.phase = 'idle';
    game.adminId = null;
  }
}

// # Hardware WebSocket Helpers
function sendHardware(payload) {
  const socket = game.hardware.socket;
  if (!socket || socket.readyState !== WebSocket.OPEN) return false;
  socket.send(JSON.stringify(payload));
  return true;
}

// # Round Lifecycle
function clearCountdownTimer() {
  if (game.timers.countdown) {
    clearTimeout(game.timers.countdown);
    game.timers.countdown = null;
  }
}

function resetRound(reason = 'reset') {
  clearCountdownTimer();
  game.phase = game.players.size ? 'lobby' : 'idle';
  game.roundId = null;
  game.calledNumbers = [];
  game.startedAt = null;
  game.endedAt = null;
  game.winner = null;
  for (const player of game.players.values()) {
    ensureMarkedIndexes(player);
    player.ready = false;
    player.card = createBingoCard();
    player.markedIndexes = new Set([12]);
  }
  pruneDisconnectedLobbyPlayers();
  sendHardware({ type: 'reset', reason });
  io.emit('round_reset', { reason });
  broadcastState();
}

function assignAdmin() {
  if (game.adminId && game.players.has(game.adminId)) return;
  const firstConnected = [...game.players.values()].find((player) => player.connected);
  game.adminId = firstConnected ? firstConnected.id : null;
}

function nextSeat() {
  const used = new Set([...game.players.values()].map((player) => player.seat));
  for (let seat = 1; seat <= MAX_PLAYERS; seat += 1) {
    if (!used.has(seat)) return seat;
  }
  return null;
}

// # Bingo Card Generation
function createBingoCard() {
  const ranges = [
    [1, 15],
    [16, 30],
    [31, 45],
    [46, 60],
    [61, 75],
  ];

  const columns = ranges.map(([min, max]) => {
    const values = [];
    for (let value = min; value <= max; value += 1) values.push(value);
    shuffle(values);
    return values.slice(0, CARD_SIZE);
  });

  const card = [];
  for (let row = 0; row < CARD_SIZE; row += 1) {
    for (let column = 0; column < CARD_SIZE; column += 1) {
      const index = row * CARD_SIZE + column;
      card.push(index === 12 ? { value: 'FREE', free: true } : { value: columns[column][row], free: false });
    }
  }
  return card;
}

function shuffle(values) {
  for (let index = values.length - 1; index > 0; index -= 1) {
    const swapIndex = Math.floor(Math.random() * (index + 1));
    [values[index], values[swapIndex]] = [values[swapIndex], values[index]];
  }
  return values;
}

// # Game Actions
function allPlayersReady() {
  return game.players.size === MAX_PLAYERS && [...game.players.values()].every((player) => player.ready);
}

function startGame(socket) {
  if (socket.id !== game.adminId) {
    socket.emit('action_error', { message: 'Only the lobby admin can start the game.' });
    return;
  }
  if (game.phase !== 'lobby' && game.phase !== 'idle') {
    socket.emit('action_error', { message: 'A round is already in progress.' });
    return;
  }
  if (game.players.size !== MAX_PLAYERS) {
    socket.emit('action_error', { message: `Exactly ${MAX_PLAYERS} players must join before starting.` });
    return;
  }
  if (!allPlayersReady()) {
    socket.emit('action_error', { message: 'All players must be ready first.' });
    return;
  }
  if (!ALLOW_SOFTWARE_ONLY && !game.hardware.connected) {
    socket.emit('action_error', { message: 'The arcade console is not connected yet.' });
    return;
  }

  clearCountdownTimer();
  game.phase = 'countdown';
  game.roundId = `${Date.now()}-${Math.random().toString(16).slice(2, 8)}`;
  game.calledNumbers = [];
  game.startedAt = null;
  game.endedAt = null;
  game.winner = null;
  for (const player of game.players.values()) {
    ensureMarkedIndexes(player);
    player.markedIndexes = new Set([12]);
  }

  sendHardware({
    type: 'start_game',
    roundId: game.roundId,
    maxBall: MAX_BALL,
    countdownMs: COUNTDOWN_MS,
  });

  io.emit('game_countdown', { roundId: game.roundId, countdownMs: COUNTDOWN_MS });
  broadcastState();

  game.timers.countdown = setTimeout(() => {
    if (game.phase !== 'countdown') return;
    game.phase = 'playing';
    game.startedAt = Date.now();
    io.emit('game_started', { roundId: game.roundId, startedAt: game.startedAt });
    broadcastState();
  }, COUNTDOWN_MS);
}

function claimBingo(socket) {
  const player = game.players.get(socket.id);
  if (!player || game.phase !== 'playing') {
    socket.emit('claim_rejected', { message: 'There is no active round to claim.' });
    return;
  }
  ensureMarkedIndexes(player);

  const result = validateClaim(player.card, [...player.markedIndexes]);
  if (!result.ok) {
    socket.emit('claim_rejected', { message: result.message });
    return;
  }

  game.phase = 'game_over';
  game.endedAt = Date.now();
  const winTimeMs = Math.max(0, game.endedAt - game.startedAt);
  game.winner = {
    id: player.id,
    name: player.name,
    seat: player.seat,
    winTimeMs,
    winTimeSeconds: Math.round(winTimeMs / 1000),
    line: result.line,
  };

  sendHardware({
    type: 'game_over',
    roundId: game.roundId,
    winnerName: player.name,
    winnerSeat: player.seat,
    winTimeMs,
  });

  io.emit('game_over', game.winner);
  broadcastState();
}

function markCell(socket, index, marked) {
  const player = game.players.get(socket.id);
  if (!player) return;
  ensureMarkedIndexes(player);

  const cellIndex = Number(index);
  if (!Number.isInteger(cellIndex) || cellIndex < 0 || cellIndex >= CARD_SIZE * CARD_SIZE) {
    socket.emit('mark_rejected', { message: 'That square is not on your Bingo card.' });
    return;
  }

  const cell = player.card[cellIndex];
  if (!cell) return;

  if (cell.free) {
    player.markedIndexes.add(cellIndex);
    socket.emit('marks_updated', { markedIndexes: [...player.markedIndexes] });
    return;
  }

  if (marked === false) {
    player.markedIndexes.delete(cellIndex);
    socket.emit('marks_updated', { markedIndexes: [...player.markedIndexes] });
    return;
  }

  if (game.phase !== 'playing') {
    socket.emit('mark_rejected', { message: 'Wait until the round starts before marking numbers.' });
    return;
  }

  const value = Number(cell.value);
  if (!game.calledNumbers.includes(value)) {
    socket.emit('mark_rejected', { message: `${value} has not been called by the arcade console yet.` });
    return;
  }

  player.markedIndexes.add(cellIndex);
  socket.emit('marks_updated', { markedIndexes: [...player.markedIndexes] });
}

// # Win Validation
function validateClaim(card, markedIndexes) {
  const marked = new Set(
    Array.isArray(markedIndexes)
      ? markedIndexes.map(Number).filter((index) => Number.isInteger(index) && index >= 0 && index < CARD_SIZE * CARD_SIZE)
      : [],
  );

  marked.add(12);
  const called = new Set(game.calledNumbers);
  const lines = getWinningLines();

  for (const line of lines) {
    const allMarked = line.every((index) => marked.has(index));
    if (!allMarked) continue;

    const allCalled = line.every((index) => {
      const cell = card[index];
      return cell.free || called.has(Number(cell.value));
    });

    if (allCalled) {
      return { ok: true, line };
    }
  }

  return {
    ok: false,
    message: 'That line is not complete with called numbers yet. Check the arcade display and try again.',
  };
}

function getWinningLines() {
  const lines = [];
  for (let row = 0; row < CARD_SIZE; row += 1) {
    lines.push([...Array(CARD_SIZE)].map((_v, column) => row * CARD_SIZE + column));
  }
  for (let column = 0; column < CARD_SIZE; column += 1) {
    lines.push([...Array(CARD_SIZE)].map((_v, row) => row * CARD_SIZE + column));
  }
  lines.push([0, 6, 12, 18, 24]);
  lines.push([4, 8, 12, 16, 20]);
  return lines;
}

// # Hardware Message Handling
function handleHardwareMessage(rawMessage) {
  let message;
  try {
    message = JSON.parse(rawMessage);
  } catch {
    sendHardware({ type: 'error', message: 'Invalid JSON message.' });
    return;
  }

  game.hardware.lastSeen = Date.now();

  if (message.type === 'hardware_hello') {
    game.hardware.deviceId = String(message.deviceId || 'esp32');
    sendHardware({ type: 'sync', ...publicState() });
    broadcastState();
    return;
  }

  if (message.type === 'drawn_number') {
    const number = Number(message.number);
    if (game.phase !== 'playing') return;
    if (!Number.isInteger(number) || number < 1 || number > MAX_BALL) return;
    if (game.calledNumbers.includes(number)) {
      sendHardware({ type: 'duplicate_draw_ignored', number });
      return;
    }

    game.calledNumbers.push(number);
    io.emit('draw_progress', { calledCount: game.calledNumbers.length });
    broadcastState();
    return;
  }

  if (message.type === 'reset_requested') {
    resetRound('hardware_button');
    return;
  }

  if (message.type === 'heartbeat') {
    sendHardware({ type: 'heartbeat_ack', serverTime: Date.now(), phase: game.phase });
  }
}

// # Browser Socket.IO Handling
io.on('connection', (socket) => {
  socket.emit('lobby_state', publicState());

  socket.on('join_lobby', ({ playerName } = {}) => {
    pruneDisconnectedLobbyPlayers();

    const existing = game.players.get(socket.id);
    if (existing) {
      existing.name = cleanName(playerName);
      existing.connected = true;
      socket.emit('player_state', playerState(socket.id));
      broadcastState();
      return;
    }

    if (game.players.size >= MAX_PLAYERS) {
      socket.emit('action_error', { message: 'The arcade lobby is full.' });
      return;
    }
    if (game.phase === 'countdown' || game.phase === 'playing') {
      socket.emit('action_error', { message: 'A round is already running. Join after reset.' });
      return;
    }

    const seat = nextSeat();
    game.players.set(socket.id, {
      id: socket.id,
      name: cleanName(playerName),
      seat,
      ready: false,
      connected: true,
      card: createBingoCard(),
      markedIndexes: new Set([12]),
      joinedAt: Date.now(),
    });

    if (!game.adminId) game.adminId = socket.id;
    if (game.phase === 'idle') game.phase = 'lobby';

    socket.emit('player_state', playerState(socket.id));
    broadcastState();
  });

  socket.on('set_ready', ({ ready } = {}) => {
    const player = game.players.get(socket.id);
    if (!player) return;
    if (game.phase !== 'lobby' && game.phase !== 'idle') return;
    player.ready = Boolean(ready);
    broadcastState();
  });

  socket.on('start_game', () => startGame(socket));
  socket.on('mark_cell', ({ index, marked } = {}) => markCell(socket, index, marked));
  socket.on('claim_bingo', () => claimBingo(socket));

  socket.on('reset_game', () => {
    if (socket.id !== game.adminId) {
      socket.emit('action_error', { message: 'Only the lobby admin can reset the round.' });
      return;
    }
    resetRound('admin_reset');
  });

  socket.on('disconnect', () => {
    const player = game.players.get(socket.id);
    if (!player) return;

    if (game.phase === 'countdown' || game.phase === 'playing') {
      player.connected = false;
    } else {
      game.players.delete(socket.id);
    }

    assignAdmin();
    if (!game.players.size) {
      game.phase = 'idle';
      game.adminId = null;
      game.calledNumbers = [];
      game.winner = null;
    }
    broadcastState();
  });
});

// # Hardware WebSocket Upgrade
server.on('upgrade', (request, socket, head) => {
  const url = new URL(request.url, `http://${request.headers.host}`);
  if (url.pathname !== '/hardware') return;

  hardwareWss.handleUpgrade(request, socket, head, (ws) => {
    hardwareWss.emit('connection', ws, request);
  });
});

// # Hardware WebSocket Connection
hardwareWss.on('connection', (ws) => {
  if (game.hardware.socket && game.hardware.socket.readyState === WebSocket.OPEN) {
    game.hardware.socket.close(1012, 'New hardware connection opened.');
  }

  game.hardware.connected = true;
  game.hardware.socket = ws;
  game.hardware.lastSeen = Date.now();
  sendHardware({ type: 'sync', ...publicState() });
  broadcastState();

  ws.on('message', (data) => handleHardwareMessage(data.toString()));
  ws.on('close', () => {
    if (game.hardware.socket === ws) {
      game.hardware.connected = false;
      game.hardware.socket = null;
      game.hardware.lastSeen = Date.now();
      broadcastState();
    }
  });
  ws.on('error', (error) => console.error('Hardware websocket error:', error.message));
});

// # Start Server
server.listen(PORT, () => {
  console.log(`Bingo arcade server listening on port ${PORT}`);
});

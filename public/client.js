const socket = io();

const screens = {
  join: document.getElementById('join-screen'),
  lobby: document.getElementById('lobby-screen'),
  countdown: document.getElementById('countdown-screen'),
  game: document.getElementById('game-screen'),
  winner: document.getElementById('winner-screen'),
};

const state = {
  lobby: null,
  player: null,
  marked: new Set([12]),
  timer: null,
  countdownTimer: null,
  countdownEndsAt: null,
};

const elements = {
  socketStatus: document.getElementById('socket-status'),
  hardwareStatus: document.getElementById('hardware-status'),
  joinForm: document.getElementById('join-form'),
  playerName: document.getElementById('player-name'),
  joinMessage: document.getElementById('join-message'),
  lobbyMessage: document.getElementById('lobby-message'),
  gameMessage: document.getElementById('game-message'),
  playersList: document.getElementById('players-list'),
  readyToggle: document.getElementById('ready-toggle'),
  startButton: document.getElementById('start-button'),
  resetLobbyButton: document.getElementById('reset-lobby-button'),
  resetGameButton: document.getElementById('reset-game-button'),
  countdownValue: document.getElementById('countdown-value'),
  bingoCard: document.getElementById('bingo-card'),
  timerDisplay: document.getElementById('timer-display'),
  calledCount: document.getElementById('called-count'),
  claimButton: document.getElementById('claim-button'),
  winnerTitle: document.getElementById('winner-title'),
  winnerDetail: document.getElementById('winner-detail'),
  playAgainButton: document.getElementById('play-again-button'),
};

elements.playerName.value = localStorage.getItem('bingo.playerName') || '';

elements.joinForm.addEventListener('submit', (event) => {
  event.preventDefault();
  const playerName = elements.playerName.value.trim();
  if (!playerName) return;
  localStorage.setItem('bingo.playerName', playerName);
  socket.emit('join_lobby', { playerName });
  setMessage(elements.joinMessage, 'Joining lobby...');
});

elements.readyToggle.addEventListener('change', () => {
  socket.emit('set_ready', { ready: elements.readyToggle.checked });
});

elements.startButton.addEventListener('click', () => {
  socket.emit('start_game');
});

elements.resetLobbyButton.addEventListener('click', () => {
  socket.emit('reset_game');
});

elements.resetGameButton.addEventListener('click', () => {
  socket.emit('reset_game');
});

elements.claimButton.addEventListener('click', () => {
  socket.emit('claim_bingo', { markedIndexes: [...state.marked] });
  setMessage(elements.gameMessage, 'Checking your card...');
});

elements.playAgainButton.addEventListener('click', () => {
  showScreen('lobby');
});

socket.on('connect', () => {
  setStatus(elements.socketStatus, 'Connected', 'ok');
});

socket.on('disconnect', () => {
  setStatus(elements.socketStatus, 'Reconnecting', 'warn');
});

socket.on('lobby_state', (lobby) => {
  state.lobby = lobby;
  renderStatus();
  renderLobby();
  routeForPhase();
});

socket.on('player_state', (player) => {
  state.player = player;
  elements.readyToggle.checked = Boolean(player.ready);
  renderCard(player.card || []);
  renderLobby();
  routeForPhase();
});

socket.on('game_countdown', ({ countdownMs }) => {
  state.marked = new Set([12]);
  clearMessage(elements.gameMessage);
  state.countdownEndsAt = Date.now() + countdownMs;
  startCountdownTicker();
  showScreen('countdown');
});

socket.on('game_started', ({ startedAt }) => {
  state.marked = new Set([12]);
  clearMessage(elements.gameMessage);
  renderCard(state.player?.card || []);
  startGameTimer(startedAt);
  showScreen('game');
});

socket.on('draw_progress', ({ calledCount }) => {
  elements.calledCount.textContent = `${calledCount} called`;
});

socket.on('claim_rejected', ({ message }) => {
  setMessage(elements.gameMessage, message, true);
});

socket.on('game_over', (winner) => {
  stopTimers();
  elements.winnerTitle.textContent = `${winner.name} wins`;
  elements.winnerDetail.textContent = `Player ${winner.seat} finished in ${formatTime(winner.winTimeMs)}.`;
  showScreen('winner');
});

socket.on('round_reset', () => {
  stopTimers();
  state.marked = new Set([12]);
  clearMessage(elements.gameMessage);
  if (state.player?.card) renderCard(state.player.card);
});

socket.on('action_error', ({ message }) => {
  const target = currentScreen() === 'game' ? elements.gameMessage : elements.lobbyMessage;
  setMessage(target, message, true);
});

function routeForPhase() {
  if (!state.player) {
    showScreen('join');
    return;
  }

  if (!state.lobby) return;
  if (state.lobby.phase === 'countdown') {
    showScreen('countdown');
  } else if (state.lobby.phase === 'playing') {
    startGameTimer(state.lobby.startedAt);
    showScreen('game');
  } else if (state.lobby.phase === 'game_over' && state.lobby.winner) {
    elements.winnerTitle.textContent = `${state.lobby.winner.name} wins`;
    elements.winnerDetail.textContent = `Player ${state.lobby.winner.seat} finished in ${formatTime(state.lobby.winner.winTimeMs)}.`;
    showScreen('winner');
  } else {
    showScreen('lobby');
  }
}

function renderStatus() {
  if (!state.lobby) return;
  const hardware = state.lobby.hardware?.connected;
  setStatus(elements.hardwareStatus, hardware ? 'Console online' : 'Console offline', hardware ? 'ok' : 'warn');
  elements.calledCount.textContent = `${state.lobby.calledCount || 0} called`;
}

function renderLobby() {
  if (!state.lobby) return;

  elements.playersList.innerHTML = '';
  for (let seat = 1; seat <= state.lobby.maxPlayers; seat += 1) {
    const player = state.lobby.players.find((candidate) => candidate.seat === seat);
    const row = document.createElement('div');
    row.className = `player-row ${player ? 'filled' : ''}`;
    row.innerHTML = player
      ? `<span class="seat">P${seat}</span><strong>${escapeHtml(player.name)}</strong><span>${player.admin ? 'Admin' : player.ready ? 'Ready' : 'Waiting'}</span>`
      : `<span class="seat">P${seat}</span><strong>Open seat</strong><span>Waiting</span>`;
    elements.playersList.appendChild(row);
  }

  const amAdmin = state.player && state.lobby.adminId === state.player.id;
  const full = state.lobby.players.length === state.lobby.maxPlayers;
  const ready = state.lobby.players.every((player) => player.ready);
  elements.startButton.hidden = !amAdmin;
  elements.resetLobbyButton.hidden = !amAdmin;
  elements.resetGameButton.hidden = !amAdmin;
  elements.startButton.disabled = !(amAdmin && full && ready && state.lobby.hardware.connected);

  if (!amAdmin) {
    setMessage(elements.lobbyMessage, 'Waiting for the lobby admin to start.');
  } else if (!state.lobby.hardware.connected) {
    setMessage(elements.lobbyMessage, 'Connect the ESP32 arcade console before starting.', true);
  } else if (!full) {
    setMessage(elements.lobbyMessage, `Waiting for ${state.lobby.maxPlayers - state.lobby.players.length} more player(s).`);
  } else if (!ready) {
    setMessage(elements.lobbyMessage, 'All players need to mark ready.');
  } else {
    setMessage(elements.lobbyMessage, 'Ready to start.');
  }
}

function renderCard(card) {
  elements.bingoCard.innerHTML = '';
  for (const [index, cell] of card.entries()) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = `bingo-cell ${cell.free || state.marked.has(index) ? 'marked' : ''}`;
    button.textContent = cell.value;
    button.disabled = Boolean(cell.free);
    button.setAttribute('aria-pressed', String(cell.free || state.marked.has(index)));
    button.addEventListener('click', () => toggleCell(index, button));
    elements.bingoCard.appendChild(button);
  }
  updateClaimButton();
}

function toggleCell(index, button) {
  if (state.marked.has(index)) {
    state.marked.delete(index);
  } else {
    state.marked.add(index);
  }
  button.classList.toggle('marked', state.marked.has(index));
  button.setAttribute('aria-pressed', String(state.marked.has(index)));
  updateClaimButton();
}

function updateClaimButton() {
  elements.claimButton.disabled = !hasMarkedLine();
}

function hasMarkedLine() {
  const lines = [];
  for (let row = 0; row < 5; row += 1) lines.push([0, 1, 2, 3, 4].map((column) => row * 5 + column));
  for (let column = 0; column < 5; column += 1) lines.push([0, 1, 2, 3, 4].map((row) => row * 5 + column));
  lines.push([0, 6, 12, 18, 24], [4, 8, 12, 16, 20]);
  return lines.some((line) => line.every((index) => state.marked.has(index)));
}

function startCountdownTicker() {
  clearInterval(state.countdownTimer);
  state.countdownTimer = setInterval(() => {
    const msLeft = Math.max(0, state.countdownEndsAt - Date.now());
    const seconds = Math.ceil(msLeft / 1000);
    elements.countdownValue.textContent = seconds > 0 ? seconds : 'GO';
    if (msLeft <= 0) clearInterval(state.countdownTimer);
  }, 150);
}

function startGameTimer(startedAt) {
  if (!startedAt) return;
  clearInterval(state.timer);
  const tick = () => {
    elements.timerDisplay.textContent = formatTime(Date.now() - startedAt);
  };
  tick();
  state.timer = setInterval(tick, 250);
}

function stopTimers() {
  clearInterval(state.timer);
  clearInterval(state.countdownTimer);
}

function showScreen(name) {
  Object.entries(screens).forEach(([screenName, element]) => {
    element.classList.toggle('active', screenName === name);
  });
}

function currentScreen() {
  return Object.entries(screens).find(([, element]) => element.classList.contains('active'))?.[0] || 'join';
}

function setStatus(element, text, kind) {
  element.textContent = text;
  element.className = `status-pill ${kind}`;
}

function setMessage(element, text, isError = false) {
  element.textContent = text;
  element.classList.toggle('error', isError);
}

function clearMessage(element) {
  setMessage(element, '');
}

function formatTime(ms) {
  const totalSeconds = Math.floor(ms / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (char) => ({
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#039;',
  }[char]));
}

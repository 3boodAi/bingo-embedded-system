let socket;
let playerName = '';
let startTime;
let gameTimer;
let calledNumbers = [];
let currentLobby = null;

document.addEventListener('DOMContentLoaded', () => {
    const mainMenuScreen = document.getElementById('main-menu-screen');
    const settingsModal = document.getElementById('settings-modal');
    const modeSelectScreen = document.getElementById('mode-select-screen');
    const nameScreen = document.getElementById('name-screen');
    const lobbyBrowserScreen = document.getElementById('lobby-browser-screen');
    const waitingRoomScreen = document.getElementById('waiting-room-screen');
    const gameScreen = document.getElementById('game-screen');
    const victoryScreen = document.getElementById('victory-screen');

    function hideAllScreens() {
        mainMenuScreen.style.display = 'none';
        modeSelectScreen.style.display = 'none';
        nameScreen.style.display = 'none';
        if (lobbyBrowserScreen) lobbyBrowserScreen.style.display = 'none';
        waitingRoomScreen.style.display = 'none';
        gameScreen.style.display = 'none';
        victoryScreen.style.display = 'none';
    }

    document.getElementById('nav-play-btn').addEventListener('click', () => {
        hideAllScreens();
        modeSelectScreen.style.display = 'flex';
        if ('speechSynthesis' in window) {
            window.speechSynthesis.speak(new SpeechSynthesisUtterance(""));
        }
    });

    document.getElementById('nav-settings-btn').addEventListener('click', () => {
        settingsModal.style.display = 'flex';
    });

    document.getElementById('close-settings-btn').addEventListener('click', () => {
        settingsModal.style.display = 'none';
    });

    document.getElementById('theme-toggle-btn').addEventListener('click', () => {
        document.body.classList.toggle('light-theme');
    });

    document.getElementById('back-to-menu-btn').addEventListener('click', () => {
        hideAllScreens();
        mainMenuScreen.style.display = 'flex';
    });

    document.getElementById('mode-sp-btn').addEventListener('click', () => {
        alert("Single player mode coming soon!");
    });

    document.getElementById('mode-mp-btn').addEventListener('click', () => {
        hideAllScreens();
        nameScreen.style.display = 'flex';
    });

    document.getElementById('back-to-mode-btn').addEventListener('click', () => {
        hideAllScreens();
        modeSelectScreen.style.display = 'flex';
    });

    const continueBtn = document.getElementById('continue-name-btn');
    if(continueBtn) {
        continueBtn.addEventListener('click', () => {
            const nameInput = document.getElementById('player-name-input').value.trim();
            if (!nameInput) {
                alert("Please enter a name!");
                return;
            }
            playerName = nameInput;
            document.getElementById('player-display').textContent = 'Player: ' + playerName;

            if (!socket) {
                socket = io();
                setupSocketListeners();
            }

            hideAllScreens();
            if (lobbyBrowserScreen) lobbyBrowserScreen.style.display = 'flex';
        });
    }

    const backToNameBtn = document.getElementById('back-to-name-btn');
    if (backToNameBtn) {
        backToNameBtn.addEventListener('click', () => {
            hideAllScreens();
            nameScreen.style.display = 'flex';
        });
    }

    const createLobbyBtn = document.getElementById('create-lobby-btn');
    if (createLobbyBtn) {
        createLobbyBtn.addEventListener('click', () => {
            if (socket) {
                socket.emit('create_lobby', { playerName: playerName });
            }
        });
    }

    document.getElementById('start-game-btn').addEventListener('click', () => {
        if (socket) {
            socket.emit('start_game');
        }
    });

    document.getElementById('claim-btn').addEventListener('click', () => {
        if ('speechSynthesis' in window) {
            const utterance = new SpeechSynthesisUtterance("BINGO! You won!");
            window.speechSynthesis.speak(utterance);
        }
        const elapsedTime = Math.floor((Date.now() - startTime) / 1000);
        if (socket) {
            socket.emit('bingo_claimed', { name: playerName, time: elapsedTime });
        }
    });

    function setupSocketListeners() {
        socket.on('lobbies_update', (lobbies) => {
            const listContainer = document.getElementById('lobbies-list');
            if (!listContainer) return;
            listContainer.innerHTML = '';
            
            if (lobbies.length === 0) {
                listContainer.innerHTML = '<p style="text-align: center; color: #888;">No active lobbies...</p>';
                return;
            }

            lobbies.forEach(lobby => {
                const item = document.createElement('div');
                item.className = 'lobby-item';
                const disabledStr = (lobby.gameActive || lobby.players.length >= lobby.maxPlayers) ? 'disabled' : '';
                item.innerHTML = '<span>' + lobby.name + ' - ' + lobby.players.length + '/' + lobby.maxPlayers + '</span>' +
                                 '<button class="lobby-join-btn" data-id="' + lobby.id + '" ' + disabledStr + '>Join</button>';
                listContainer.appendChild(item);
            });

            document.querySelectorAll('.lobby-join-btn').forEach(btn => {
                btn.addEventListener('click', (e) => {
                    const lobbyId = e.target.getAttribute('data-id');
                    socket.emit('join_lobby', { lobbyId, playerName });
                });
            });
        });

        socket.on('lobby_joined', (lobby) => {
            currentLobby = lobby;
            hideAllScreens();
            waitingRoomScreen.style.display = 'flex';
            updateWaitingRoom(lobby);
        });

        socket.on('lobby_updated', (lobby) => {
            currentLobby = lobby;
            if (waitingRoomScreen.style.display === 'flex' || waitingRoomScreen.style.display === 'block') {
                updateWaitingRoom(lobby);
            }
        });

        socket.on('lobby_error', (data) => {
            alert(data.message);
        });

        socket.on('game_started', () => {
            hideAllScreens();
            gameScreen.style.display = 'flex';
            startTime = Date.now();
            initGame();
        });

        const handleNewNumber = (data) => {
            const num = typeof data === 'object' ? (data.number || data.value || Object.values(data)[0]) : data;
            calledNumbers.push(Number(num));
            document.getElementById('latest-number').textContent = 'Latest Number: ' + num;
            speakNumber(num);
        };

        socket.on('new_number', handleNewNumber);

        socket.on('game_over', (data) => {
            clearInterval(gameTimer);
            
            gameScreen.style.display = 'none';
            victoryScreen.style.display = 'flex';
            
            const winnerName = data.winner || 'Unknown';
            const winTime = data.time || 0;
            
            document.getElementById('winner-info').innerHTML = 'Winner: ' + winnerName + '<br><br>Time: ' + winTime + ' seconds';

            if ('speechSynthesis' in window) {
                const utterance = new SpeechSynthesisUtterance("BINGO! We have a winner!");
                utterance.pitch = 1.2;
                utterance.rate = 1.1;
                window.speechSynthesis.speak(utterance);
            }
        });
    }

    function updateWaitingRoom(lobby) {
        if (!lobby) return;
        const playerList = document.getElementById('player-list');
        playerList.innerHTML = '';
        lobby.players.forEach(p => {
            const pEl = document.createElement('p');
            pEl.textContent = p.name + (p.id === lobby.leader ? ' (Leader)' : '');
            pEl.style.margin = '5px 0';
            playerList.appendChild(pEl);
        });

        const startBtn = document.getElementById('start-game-btn');
        if (socket.id === lobby.leader) {
            startBtn.style.display = 'block';
        } else {
            startBtn.style.display = 'none';
        }
    }
});

let currentGridSize = 5;

function initGame() {
    const sizeSelect = document.getElementById('grid-size-select');
    if (sizeSelect) {
        currentGridSize = parseInt(sizeSelect.value, 10);
    }
    const bingoNumbers = generateBingoNumbers(currentGridSize * currentGridSize);
    renderBoard(bingoNumbers, currentGridSize);
    startTimer();
}

function startTimer() {
    gameTimer = setInterval(() => {
        const elapsed = Math.floor((Date.now() - startTime) / 1000);
        const mins = String(Math.floor(elapsed / 60)).padStart(2, '0');
        const secs = String(elapsed % 60).padStart(2, '0');
        document.getElementById('timer-display').textContent = 'Time: ' + mins + ':' + secs;
    }, 1000);
}

function generateBingoNumbers(count) {
    const numbers = new Set();
    while (numbers.size < count) {
        numbers.add(Math.floor(Math.random() * 100));
    }
    return Array.from(numbers);
}

function renderBoard(numbers, size) {
    const board = document.getElementById('bingo-grid');
    board.style.gridTemplateColumns = 'repeat(' + size + ', 1fr)';
    board.style.gridTemplateRows = 'repeat(' + size + ', 1fr)';
    board.innerHTML = ''; 
    
    numbers.forEach((number, index) => {
        const cell = document.createElement('div');
        cell.className = 'bingo-cell';
        cell.textContent = number;
        cell.dataset.index = index;
        cell.style.animationDelay = (index * 0.03) + 's'; 
        
        cell.addEventListener('click', () => {
            const cellNum = parseInt(cell.innerText);
            if (!calledNumbers.includes(cellNum)) return;
            cell.classList.toggle('marked');
            checkClaimEligibility(size);
        });
        
        board.appendChild(cell);
    });
}

function checkClaimEligibility(size) {
    const cells = Array.from(document.querySelectorAll('.bingo-cell'));
    const isMarked = (idx) => cells[idx].classList.contains('marked');
    
    let hasBingo = false;

    for (let r = 0; r < size; r++) {
        let rowWin = true;
        for (let c = 0; c < size; c++) {
            if (!isMarked(r * size + c)) rowWin = false;
        }
        if (rowWin) hasBingo = true;
    }
    
    for (let c = 0; c < size; c++) {
        let colWin = true;
        for (let r = 0; r < size; r++) {
            if (!isMarked(r * size + c)) colWin = false;
        }
        if (colWin) hasBingo = true;
    }
    
    let diag1Win = true;
    let diag2Win = true;
    for (let i = 0; i < size; i++) {
        if (!isMarked(i * size + i)) diag1Win = false;
        if (!isMarked(i * size + (size - 1 - i))) diag2Win = false;
    }
    if (diag1Win || diag2Win) hasBingo = true;

    document.getElementById('claim-btn').disabled = !hasBingo;
}

function speakNumber(num) {
    if ('speechSynthesis' in window) {
        const utterance = new SpeechSynthesisUtterance('Number ' + num);
        window.speechSynthesis.speak(utterance);
    }
}

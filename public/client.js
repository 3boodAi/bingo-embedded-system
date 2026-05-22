let socket;
let playerName = '';
let startTime;
let gameTimer;
let calledNumbers = [];

document.addEventListener('DOMContentLoaded', () => {
    const mainMenuScreen = document.getElementById('main-menu-screen');
    const settingsModal = document.getElementById('settings-modal');
    const modeSelectScreen = document.getElementById('mode-select-screen');
    const nameScreen = document.getElementById('name-screen');
    const waitingRoomScreen = document.getElementById('waiting-room-screen');
    const gameScreen = document.getElementById('game-screen');
    const victoryScreen = document.getElementById('victory-screen');

    function hideAllScreens() {
        mainMenuScreen.style.display = 'none';
        modeSelectScreen.style.display = 'none';
        nameScreen.style.display = 'none';
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

    document.getElementById('join-lobby-btn').addEventListener('click', () => {
        const nameInput = document.getElementById('player-name-input').value.trim();
        if (!nameInput) {
            alert("Please enter a name!");
            return;
        }
        playerName = nameInput;
        document.getElementById('player-display').textContent = 'Player: ' + playerName;

        hideAllScreens();
        waitingRoomScreen.style.display = 'flex';
    });

    document.getElementById('start-game-btn').addEventListener('click', () => {
        hideAllScreens();
        gameScreen.style.display = 'flex';

        // 3. Connect to server
        socket = io();

        socket.on('connect', () => {
            socket.emit('register_player');
        });

        const handleNewNumber = (data) => {
            const num = typeof data === 'object' ? (data.number || data.value || Object.values(data)[0]) : data;
            calledNumbers.push(Number(num));
            document.getElementById('latest-number').textContent = `Latest Number: ${num}`;
            speakNumber(num);
        };

        socket.on('new_number', handleNewNumber);
        socket.on('number_generated', handleNewNumber);

        socket.on('game_over', (data) => {
            clearInterval(gameTimer);
            
            gameScreen.style.display = 'none';
            victoryScreen.style.display = 'flex';
            
            const winnerName = data.winner || 'Unknown';
            const winTime = data.time || 0;
            
            document.getElementById('winner-info').innerHTML = `Winner: ${winnerName}<br><br>Time: ${winTime} seconds`;

            if ('speechSynthesis' in window) {
                const utterance = new SpeechSynthesisUtterance(`BINGO! We have a winner!`);
                utterance.pitch = 1.2;
                utterance.rate = 1.1;
                window.speechSynthesis.speak(utterance);
            }
        });
        
        // 4. Start the local timer and initialize the board
        startTime = Date.now();
        initGame();
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
});

function initGame() {
    const bingoNumbers = generateBingoNumbers();
    renderBoard(bingoNumbers);
    startTimer();
}

function startTimer() {
    gameTimer = setInterval(() => {
        const elapsed = Math.floor((Date.now() - startTime) / 1000);
        const mins = String(Math.floor(elapsed / 60)).padStart(2, '0');
        const secs = String(elapsed % 60).padStart(2, '0');
        document.getElementById('timer-display').textContent = `Time: ${mins}:${secs}`;
    }, 1000);
}

function generateBingoNumbers() {
    const numbers = new Set();
    while (numbers.size < 25) {
        numbers.add(Math.floor(Math.random() * 100));
    }
    return Array.from(numbers);
}

function renderBoard(numbers) {
    const board = document.getElementById('bingo-grid');
    board.innerHTML = ''; 
    
    numbers.forEach((number, index) => {
        const cell = document.createElement('div');
        cell.className = 'bingo-cell';
        cell.textContent = number;
        cell.dataset.index = index;
        
        cell.addEventListener('click', () => {
            const cellNum = parseInt(cell.innerText);
            if (!calledNumbers.includes(cellNum)) return;
            cell.classList.toggle('marked');
            checkClaimEligibility();
        });
        
        board.appendChild(cell);
    });
}

function checkClaimEligibility() {
    const cells = Array.from(document.querySelectorAll('.bingo-cell'));
    const isMarked = (idx) => cells[idx].classList.contains('marked');
    
    let hasBingo = false;

    for (let r = 0; r < 5; r++) {
        if ([0, 1, 2, 3, 4].every(c => isMarked(r * 5 + c))) hasBingo = true;
    }
    for (let c = 0; c < 5; c++) {
        if ([0, 1, 2, 3, 4].every(r => isMarked(r * 5 + c))) hasBingo = true;
    }
    if ([0, 6, 12, 18, 24].every(isMarked)) hasBingo = true;
    if ([4, 8, 12, 16, 20].every(isMarked)) hasBingo = true;

    document.getElementById('claim-btn').disabled = !hasBingo;
}

function speakNumber(num) {
    if ('speechSynthesis' in window) {
        const utterance = new SpeechSynthesisUtterance(`Number ${num}`);
        window.speechSynthesis.speak(utterance);
    }
}
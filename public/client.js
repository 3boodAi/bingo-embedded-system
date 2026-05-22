const socket = io; // Note: Not invoked yet.
let socketInstance;
let playerName = '';
let startTime;
let gameTimer;
let calledNumbers = [];

document.addEventListener('DOMContentLoaded', () => {
    const lobbyScreen = document.getElementById('lobby-screen');
    const gameScreen = document.getElementById('game-screen');
    const victoryScreen = document.getElementById('victory-screen');

    document.getElementById('join-btn').addEventListener('click', () => {
        const inputName = document.getElementById('player-name-input').value.trim();
        playerName = inputName || "Player_" + Math.floor(Math.random() * 1000);
        document.getElementById('playerName').textContent = playerName;

        if ('speechSynthesis' in window) {
            window.speechSynthesis.speak(new SpeechSynthesisUtterance(""));
        }

        lobbyScreen.style.display = 'none';
        gameScreen.style.display = 'flex';

        socketInstance = io();

        socketInstance.on('connect', () => {
            socketInstance.emit('register_player');
        });

        const handleNewNumber = (data) => {
            const num = typeof data === 'object' ? (data.number || data.value || Object.values(data)[0]) : data;
            calledNumbers.push(Number(num));
            displayNewNumber(num);
            speakNumber(num);
        };

        socketInstance.on('new_number', handleNewNumber);
        socketInstance.on('number_generated', handleNewNumber);

        socketInstance.on('game_over', (data) => {
            clearInterval(gameTimer);
            
            gameScreen.style.display = 'none';
            victoryScreen.style.display = 'flex';
            
            const winnerName = data.name || data.data?.name || 'Unknown';
            const winTime = data.time || data.data?.time || 0;
            
            document.getElementById('victory-message').innerHTML = `Winner: ${winnerName}<br><br>Time: ${winTime} seconds`;

            if ('speechSynthesis' in window) {
                const utterance = new SpeechSynthesisUtterance(`BINGO! We have a winner!`);
                utterance.pitch = 1.2;
                utterance.rate = 1.1;
                window.speechSynthesis.speak(utterance);
            }
        });

        startTime = Date.now();
        initGame();
    });

    document.getElementById('claimBingoBtn').addEventListener('click', () => {
        if ('speechSynthesis' in window) {
            const utterance = new SpeechSynthesisUtterance("BINGO! You won!");
            window.speechSynthesis.speak(utterance);
        }
        const elapsedTime = Math.floor((Date.now() - startTime) / 1000);
        if (socketInstance) {
            socketInstance.emit('bingo_claimed', { name: playerName, time: elapsedTime });
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
        document.getElementById('gameTimer').textContent = `${mins}:${secs}`;
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
    const board = document.getElementById('bingoBoard');
    board.innerHTML = ''; 
    
    numbers.forEach((number, index) => {
        const cell = document.createElement('div');
        cell.className = 'bingo-cell';
        cell.textContent = number;
        cell.dataset.index = index;
        
        cell.addEventListener('click', () => {
            if (!calledNumbers.includes(Number(number))) return;
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

    document.getElementById('claimBingoBtn').disabled = !hasBingo;
}

function displayNewNumber(num) {
    let displayEl = document.getElementById('latestNumberDisplay');
    if (!displayEl) {
        displayEl = document.createElement('div');
        displayEl.id = 'latestNumberDisplay';
        displayEl.style.fontSize = '1.5rem';
        displayEl.style.color = 'var(--accent-color)';
        displayEl.style.textAlign = 'center';
        displayEl.style.marginBottom = '15px';
        document.querySelector('.status-panel').insertAdjacentElement('afterend', displayEl);
    }
    displayEl.textContent = `Latest Number: ${num}`;
}

function speakNumber(num) {
    if ('speechSynthesis' in window) {
        const utterance = new SpeechSynthesisUtterance(`Number ${num}`);
        window.speechSynthesis.speak(utterance);
    }
}
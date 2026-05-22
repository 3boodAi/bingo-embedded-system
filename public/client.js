const socket = io();

let playerName = '';
let startTime = Date.now();
let timerInterval;
let calledNumbers = [];

document.addEventListener('DOMContentLoaded', () => {
    initGame();

    socket.on('connect', () => {
        socket.emit('register_player');
    });

    const handleNewNumber = (data) => {
        const num = typeof data === 'object' ? (data.number || data.value || Object.values(data)[0]) : data;
        calledNumbers.push(Number(num));
        displayNewNumber(num);
        speakNumber(num);
    };

    socket.on('new_number', handleNewNumber);
    socket.on('number_generated', handleNewNumber);

    socket.on('game_over', (data) => {
        alert(`Game Over! Winner: ${data.winner || data.data?.name || 'Unknown'}`);
    });

    document.getElementById('claimBingoBtn').addEventListener('click', () => {
        if ('speechSynthesis' in window) {
            const utterance = new SpeechSynthesisUtterance("BINGO! You won!");
            window.speechSynthesis.speak(utterance);
        }
        const elapsedTime = Math.floor((Date.now() - startTime) / 1000);
        socket.emit('bingo_claimed', { name: playerName, time: elapsedTime });
    });
});

function initGame() {
    promptForName();
    const bingoNumbers = generateBingoNumbers();
    renderBoard(bingoNumbers);
    startTimer();
}

function startTimer() {
    timerInterval = setInterval(() => {
        const elapsed = Math.floor((Date.now() - startTime) / 1000);
        const mins = String(Math.floor(elapsed / 60)).padStart(2, '0');
        const secs = String(elapsed % 60).padStart(2, '0');
        document.getElementById('gameTimer').textContent = `${mins}:${secs}`;
    }, 1000);
}

function promptForName() {
    let name = prompt("Welcome to IoT Interactive Bingo!\nPlease enter your name:");
    if (!name || name.trim() === "") {
        name = "Player_" + Math.floor(Math.random() * 1000);
    }
    playerName = name.trim();
    document.getElementById('playerName').textContent = playerName;
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

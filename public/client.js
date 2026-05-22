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
  const nameInput = document.getElementById('player-name-input').value.trim();
  if (!nameInput) {
    alert("Please enter a name!");
    return;
  }
  playerName = nameInput;

  // 1. Unblock browser audio policy with a silent utterance
  window.speechSynthesis.speak(new SpeechSynthesisUtterance(""));

  // 2. Switch UI screens
  document.getElementById('lobby-screen').style.display = 'none';
  document.getElementById('game-screen').style.display = 'block';

  // 3. Connect to server
  socket = io();
  
  // 4. Start the local timer (assuming startGameTimer() exists)
  if (typeof startGameTimer === 'function') startGameTimer();
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
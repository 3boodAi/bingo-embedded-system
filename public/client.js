document.addEventListener('DOMContentLoaded', () => {
    initGame();
});

let playerName = '';

function initGame() {
    promptForName();
    const bingoNumbers = generateBingoNumbers();
    renderBoard(bingoNumbers);
}

function promptForName() {
    // Prompt the user for their name via standard browser prompt
    let name = prompt("Welcome to IoT Interactive Bingo!\nPlease enter your name:");
    
    // Fallback if they cancel or enter empty string
    if (!name || name.trim() === "") {
        name = "Player_" + Math.floor(Math.random() * 1000);
    }
    
    playerName = name.trim();
    document.getElementById('playerName').textContent = playerName;
}

function generateBingoNumbers() {
    const numbers = new Set();
    
    // Generate 25 unique random numbers between 0 and 99
    while (numbers.size < 25) {
        const randomNum = Math.floor(Math.random() * 100);
        numbers.add(randomNum);
    }
    
    return Array.from(numbers);
}

function renderBoard(numbers) {
    const board = document.getElementById('bingoBoard');
    board.innerHTML = ''; // Clear the board before rendering
    
    numbers.forEach(number => {
        const cell = document.createElement('div');
        cell.className = 'bingo-cell';
        cell.textContent = number;
        
        // Add click listener to toggle the marked state visually
        cell.addEventListener('click', () => {
            cell.classList.toggle('marked');
            checkClaimEligibility();
        });
        
        board.appendChild(cell);
    });
}

function checkClaimEligibility() {
    // Basic logic just to turn the claim button on/off if items are marked.
    // Real validation would happen later in the phases.
    const markedCells = document.querySelectorAll('.bingo-cell.marked');
    const claimBtn = document.getElementById('claimBingoBtn');
    
    // Arbitrary check for demonstration: enable if at least 5 cells are marked
    if (markedCells.length >= 5) {
        claimBtn.disabled = false;
    } else {
        claimBtn.disabled = true;
    }
}
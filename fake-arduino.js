const { io } = require("socket.io-client");

const socket = io("https://bingo-embedded-system.onrender.com/");

socket.on("connect", () => {
    console.log("Fake Arduino Connected to Server!");

    setInterval(() => {
        const randomNumber = Math.floor(Math.random() * 100);
        socket.emit("number_generated", randomNumber);
        console.log(`Generated and sent number: ${randomNumber}`);
    }, 6000);
});

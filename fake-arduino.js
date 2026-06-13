const { io } = require("socket.io-client");

const socket = io(process.env.SERVER_URL || "http://localhost:3000/");

socket.on("connect", () => {
    console.log("Fake Arduino Connected to Server!");

    let drawnNumbers = [];
    setInterval(() => {
      if (drawnNumbers.length >= 100) return; // All numbers drawn
      let num;
      do { num = Math.floor(Math.random() * 100); } while (drawnNumbers.includes(num));
      drawnNumbers.push(num);
      console.log("Drawing:", num);
      socket.emit("number_generated", num);
    }, 6000);
});

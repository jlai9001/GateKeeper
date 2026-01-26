require('dotenv').config();

const express = require('express');

const app = express();
const PORT = 5000 || process.env.PORT;

// homepage
app.get('',(req,res)=> {
    res.send("Gate Keeper");
})

app.listen(PORT, ()=> {
    console.log(`App listening on port ${PORT}`);
})

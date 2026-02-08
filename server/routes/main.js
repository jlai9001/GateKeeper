const express = require ('express');
const router = express.Router();

// Routes - Page Navigation

router.get('',(req,res)=> {
    res.render('index');
})

router.get('/objectives',(req,res)=> {
    res.render('pages/objectives');
})

router.get('/logic',(req,res)=> {
    res.render('pages/logic');
})

router.get('/solar',(req,res)=> {
    res.render('pages/solar');
})

router.get('/electronics',(req,res)=> {
    res.render('pages/electronics');
})

router.get('/sensors',(req,res)=> {
    res.render('pages/sensors');
})

router.get('/wiring',(req,res)=> {
    res.render('pages/wiring');
})

router.get('/programming',(req,res)=> {
    res.render('pages/programming');
})

router.get('/video',(req,res)=> {
    res.render('pages/video');
})




// export router
module.exports = router;

const express = require ('express');
const router = express.Router();

// Routes - Page Navigation

router.get('',(req,res)=> {
    res.render('index');
})

router.get('/objectives', (req, res) => {
  res.render('pages/objectives', { navLabel: 'Objectives' });
});

router.get('/logic',(req,res)=> {
    res.render('pages/logic', { navLabel: 'Logic' });
})

router.get('/solar',(req,res)=> {
    res.render('pages/solar', { navLabel: 'Solar' });
})

router.get('/electronics',(req,res)=> {
    res.render('pages/electronics', { navLabel: 'Electronics' });
})

router.get('/sensors',(req,res)=> {
    res.render('pages/sensors', { navLabel: 'Sensors' });
})

router.get('/wiring',(req,res)=> {
    res.render('pages/wiring', { navLabel: 'Wiring' });
})

router.get('/programming',(req,res)=> {
    res.render('pages/programming', { navLabel: 'Programming' });
})

router.get('/video',(req,res)=> {
    res.render('pages/video', { navLabel: 'Video' });
})




// export router
module.exports = router;

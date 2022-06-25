"use strict";

/* jshint esversion: 6 */

class Holes {
    constructor() {
        this.read();
    }

    read() {
        // get Number of holes from input field
        let n = parseInt(document.getElementById("holes").value);
        let update = true ;
        if (n<1) {
            // bad input
            n = 5 ;
        } else if (n<2) {
            n = 2;
        } else if ( n>12 ) {
            n = 12;
        } else {
            update = false;
        }
        if (update) {
            document.getElementById("holes").value=n;
        }
        this.holes = n;
    }

    get value() {
        return this.holes;
    }

    change() {
        let h = this.holes ;
        this.read();
        return ( h != this.holes ) ;
    }
}
var H = new Holes() ;

class Gametype {
    constructor() {
        this.ids = ["turns","sneak"].map( e=>document.getElementById(e) );
        this.read();
    }

    read() {
        // get Game type from input field(s)
        this.gametype = this.ids.filter(i=>i.checked)[0].id ;
    }

    get value() {
        return this.gametype ;
    }

    change() {
        let g = this.gametype ;
        this.read();
        return ( g != this.gametype ) ;
    }
}
var G = new Gametype() ;

class SneakGame {
    constructor () {
    }

    start () {
        this.inspections = [];
        this.date = 0;
        let current_fox = Array(H.value).fill(true);
        this.fox_history = [current_fox] ;
    }

    next( hole ) {
        // inspections are 0-based
        this.date += 1;
        this.inspections[this.date] = hole ;
        // use previous fox locations
        let old_locations = this.fox_history[this.date-1] ;
        // exclude inspected hole
        old_locations[hole] = false ;
        // sneak left
        let current_fox = old_locations.slice(1) ;
        current[H.value-1] = false;
        // sneak right
        for ( let h = 0 ; h < H.value-1 ; ++h ) {
            current_fox[h] |= old_locations[h+1] ;
        }
        // exclude inspected hole
        current_fox[hole] = false ;
        // store
        this.fox_history[this.date] = current_fox;
    }

    get foxes() {
        return this.fox_history[this.date] ;
    }

    back() { // backup a move
        this.date -= 1 ;
    }

    get number() { // of foxes left
        return this.fox_history[this.date].filter(f=>f).length ;
    }
}


class Table {
    constructor() {
        this.table = document.querySelector("table") ;
        this.thead = document.querySelector("thead") ;
        this.tbody = document.querySelector("tbody") ;
        this.day = 0 ;
        this.clear() ;
    }

    clear() {
        this.day = 0;
        this.inspect = [];
        this.header();
        this.backbutton();
    }

    backbutton() {
        let r = document.createElement("tr");
        let d = document.createElement("td");
        let b = document.createElement("button");
        b.innerText = "Back" ;
        d.appendChild(b);
        r.appendChild(d);
        this.tbody.appendChild(r);
        return r;
    }

    header() {
        this.thead.innerHTML = "";
        let r = document.createElement("tr");
        let h = document.createElement("th");
        h.innerText = "Day" ;
        r.appendChild(h) ;
        for ( let i = 1 ; i <= H.value ; ++i ) {
            h = document.createElement("th");
            h.innerText = i + "" ;
            r.appendChild(h) ;
        }
        this.thead.appendChild(r) ;
    }

    

    row() {
        if ( this.day == 0 ) {}
    }

    Tset() {
        // get Number of holes from input field
        let n = parseInt(document.getElementById("holes").value);
        if (n<1) {
            // bad input
            n = 5 ;
            document.getElementById("holes").value=n;
        }
        this.holes = n;

        // get game type from input field
    }
}
var T = new Table();            

function changeInput() {
    if ( G.change || H.change ) {
        T.Tset() ;
    }
}

// Application starting point
window.onload = () => {
    // Initial splash screen

    // Stuff into history to block browser BACK button
    window.history.pushState({}, '');
    window.addEventListener('popstate', ()=>window.history.pushState({}, '') );

    // Service worker (to manage cache for off-line function)
    if ( 'serviceWorker' in navigator ) {
        navigator.serviceWorker
        .register('/sw.js')
        .catch( err => console.log(err) );
    }
    
};

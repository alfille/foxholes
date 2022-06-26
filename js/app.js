"use strict";

/* jshint esversion: 6 */

class Holes {
    constructor() {
        this.item = document.getElementById("holes");
        this.read();
    }

    read() {
        // get Number of holes from input field
        let n = parseInt(this.item.value);
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
            this.item.value=n;
        }
        this.holes = n;
    }

    get change() {
        let h = this.holes ;
        this.read();
        return ( h != this.holes ) ;
    }

    get value() {
        return this.holes;
    }
}
var H = new Holes() ;

class Game {
    constructor () {
        this.start();
    }

    start () {
        this.inspections = [];
        this.date = 0;
        let current_fox = Array(H.value).fill(true);
        this.fox_history = [current_fox] ;
        this.inspections = [] ;
    }

    move( hole ) {
        // inspections are 0-based
        this.inspections[this.date] = hole ;
        this.date += 1;
        // use previous fox locations
        let old_locations = this.fox_history[this.date-1] ;
        // exclude inspected hole
        old_locations[hole] = false ;
        // sneak left
        let current_fox = old_locations.slice(1) ;
        current_fox[H.value-1] = false;
        // sneak right
        for ( let h = 0 ; h < H.value-1 ; ++h ) {
            current_fox[h+1] ||= old_locations[h] ;
        }
        // store
        this.fox_history[this.date] = current_fox;
    }

    get foxes() {
        return this.fox_history[this.date] ;
    }

    get prior() {
        return [this.inspections[this.date-1],this.fox_history[this.date-1]];
    }

    back() { // backup a move
        this.date -= 1 ;
    }

    get number() { // of foxes left
        return this.fox_history[this.date].filter(f=>f).length ;
    }

    get day() {
        return this.date;
    }
}
var G = new Game();

class Table {
    constructor() {
        this.table = document.querySelector("table") ;
        this.thead = document.querySelector("thead") ;
        this.tbody = document.querySelector("tbody") ;
        this.start() ;
    }

    start() {
        this.header();
        G.start();
        this.tbody.innerHTML = "";
        this.control_row();
    }

    control_row() {
        let f = G.foxes ;
        let r = document.createElement("tr");
        for ( let i = 0; i <= H.value ; ++i ) {
            let d = document.createElement("td");
            if ( i==0 ) {
                let b = document.createElement("button");
                b.innerText = "Back" ;
                b.onclick = () => T.back() ;
                d.appendChild(b);
            } else {
                d.innerHTML = (f[i-1] ? "&#129418" : "&nbsp;") + "<br>" ;
                let b = document.createElement("button");
                b.innerText = "?" ;
                b.onclick = () => T.move(i-1) ;
                d.appendChild(b);
            }
            r.appendChild(d);
        }
        let d = document.createElement("td");
        this.tbody.appendChild(r);
    }

    back() {
        if ( G.day == 0 ) {
            this.start() ;
        }
        this.remove_row();
        this.remove_row();
        G.back();
        this.control_row();
    }

    move(hole) { // hole 0-based
        G.move(hole);
        this.remove_row();
        this.static_row();
        if ( G.number == 0 ) {
            this.victory_row();
        } else {
            this.control_row();
        }
    }

    static_row() {
        let [m,f] = G.prior ;
        let r = document.createElement("tr");
        for ( let i = 0; i <= H.value ; ++i ) {
            let h = i-1; // actual hole index after first column
            let d = document.createElement("td");
            if ( i==0 ) {
                d.innerHTML = `Day ${G.day-1}`;
            } else if ( m == h ) {
                d.innerHTML = "&#x274c" ;
            } else if (f[h]) {
                d.innerHTML = "&#129418" ;
            } else {
                d.innerHTML = "&nbsp;" ;
            }
            r.appendChild(d);
        }
        let d = document.createElement("td");
        this.tbody.appendChild(r);
    }

    remove_row() {
        this.tbody.removeChild( this.tbody.lastChild ) ;
    }

    victory_row() {
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
    
}
var T = new Table();            

function changeInput() {
    if ( H.change ) {
        T.start() ;
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

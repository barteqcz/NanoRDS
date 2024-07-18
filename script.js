function hamburgerMenu() {
    let x = document.getElementById("myLinks");
    let y = document.getElementById("main")
    if (x.style.display === "flex") {
      x.style.display = "none";
      y.style.marginTop = "100px";
    } else {
      x.style.display = "flex";
      y.style.marginTop = "100px";
    }
}

function hamburgerMenuHideOnly() {
    let x = document.getElementById("myLinks");
    let y = document.getElementById("main")
    x.style.display = "none";
    y.style.marginTop = "200px";
}
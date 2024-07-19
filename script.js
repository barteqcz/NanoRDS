document.addEventListener('DOMContentLoaded', function() {
    let rdsTypeToggle = document.getElementById('rds-type-toggle');
    let rds = document.getElementById('rds');
    let rbds = document.getElementById('rbds');

    function toggleVisibility() {
        if (rdsTypeToggle.checked) {
            rds.style.display = 'none';
            rbds.style.display = 'block';
        } else {
            rds.style.display = 'block';
            rbds.style.display = 'none';
        }
    }

    toggleVisibility();

    rdsTypeToggle.addEventListener('change', toggleVisibility);
});

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
description("Ensure correct behavior of canvas with path stroke + shadow after scaling. A blue and red checkered pattern should be displayed.");

function print(message, color)
{
    var paragraph = document.createElement("div");
    paragraph.appendChild(document.createTextNode(message));
    paragraph.style.fontFamily = "monospace";
    if (color)
        paragraph.style.color = color;
    document.getElementById("console").appendChild(paragraph);
}

function shouldBeAround(a, b)
{
    var evalA;
    try {
        evalA = eval(a);
    } catch(e) {
        evalA = e;
    }

    if (Math.abs(evalA - b) < 20)
        print("PASS " + a + " is around " + b , "green")
    else
        print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
}

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '600');
canvas.setAttribute('height', '600');
var ctx = canvas.getContext('2d');

ctx.scale(2, 2);
ctx.shadowOffsetX = 100;
ctx.shadowOffsetY = 100;
ctx.strokeStyle = 'rgba(0, 0, 255, 1)';
ctx.lineWidth = 5;

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.beginPath();
ctx.moveTo(50, 50);
ctx.lineTo(100, 50);
ctx.lineTo(100, 100);
ctx.lineTo(50, 100);
ctx.lineTo(50, 50);
ctx.stroke();

ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
ctx.beginPath();
ctx.moveTo(50, 150);
ctx.lineTo(100, 150);
ctx.lineTo(100, 200);
ctx.lineTo(50, 200);
ctx.lineTo(50, 150);
ctx.stroke();

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.shadowBlur = 10;
ctx.beginPath();
ctx.moveTo(150, 50);
ctx.lineTo(200, 50);
ctx.lineTo(200, 100);
ctx.lineTo(150, 100);
ctx.lineTo(150, 50);
ctx.stroke();

ctx.shadowColor = 'rgba(255, 0, 0, 0.6)';
ctx.beginPath();
ctx.moveTo(150, 150);
ctx.lineTo(200, 150);
ctx.lineTo(200, 200);
ctx.lineTo(150, 200);
ctx.lineTo(150, 150);
ctx.stroke();

var d; // imageData.data

// Verify solid shadow.
d = ctx.getImageData(250, 200, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(300, 290, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(200, 250, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

// Verify solid alpha shadow.
d = ctx.getImageData(201, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

d = ctx.getImageData(201, 500, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

d = ctx.getImageData(300, 499, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

// Verify blurry shadow.
d = ctx.getImageData(398, 210, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '200');

d = ctx.getImageData(508, 250, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '49');

d = ctx.getImageData(450, 198, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '199');

// Verify blurry alpha shadow.
d = ctx.getImageData(505, 450, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '70');

d = ctx.getImageData(505, 450, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '70');

d = ctx.getImageData(450, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '69');

document.getElementById("captureBtn").addEventListener("click", () => {
    fetch("/api/camera/capture")
        .then(response => response.blob())
        .then(blob => {
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = "esp32_snapshot.jpg"; // Force specified filename
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        URL.revokeObjectURL(url);
    })
    .catch(error => console.error("Error downloading image:", error));
});

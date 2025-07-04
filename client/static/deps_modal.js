let fileModal = document.getElementById('proc_info_modal')

function nextFiles(maxResults=50) {
    let deps_div = fileModal.querySelector('.module_filename');
    let opens_div = fileModal.querySelector('.file_contents');
    let files_pages = fileModal.querySelector('.files_page_numbers');
    let page = parseInt(files_pages.innerText.split("/")[0]) - 1;
    if (page < (parseInt(files_pages.innerText.split("/")[1]) - 1)) {
        page++;
        let request = `${ctx}deps_for?path=${deps_div.innerText}&page=${page}&entries-per-page=${maxResults}&cached=true`;
        $.get(request, function (data) {
            Array.from(opens_div.getElementsByTagName("p")).forEach(element => {
                element.remove();
            });
            for (d in data["entries"]) {
                opens_div.innerHTML += "<p  class='file_mode_r'>" + data["entries"][d] + "</p>"
            }
            files_pages.innerText = (page + 1) + "/" + Math.ceil(data.count/maxResults).toString()
            if (page > 0) {
                fileModal.querySelector('#prevFilesButton').removeAttribute('disabled');
            }
            if (page === parseInt(files_pages.innerText.split("/")[1]) - 1) {
                fileModal.querySelector('#nextFilesButton').setAttribute('disabled', '');
            }
        });
    }
}

function prevFiles(maxResults=50) {
    let deps_div = fileModal.querySelector('.module_filename');
    let opens_div = fileModal.querySelector('.file_contents');
    let files_pages = fileModal.querySelector('.files_page_numbers');
    let page = parseInt(files_pages.innerText.split("/")[0]) - 1;
    if (page > 0) {
        page--;
        let request = `${ctx}deps_for?path=${deps_div.innerText}&page=${page}&entries-per-page=${maxResults}&cached=true`;
        $.get(request, function (data) {
            Array.from(opens_div.getElementsByTagName("p")).forEach(element => {
                element.remove();
            });
            for (d in data["entries"]) {
                opens_div.innerHTML += "<p  class='file_mode_r'>" + data["entries"][d] + "</p>"
            }
            files_pages.innerText = (page + 1) + "/" + Math.ceil(data.count/maxResults).toString()
            if (page === 0) {
                fileModal.querySelector('#prevFilesButton').setAttribute('disabled', '');
            }
            if (page < parseInt(files_pages.innerText.split("/")[1]) - 1) {
                fileModal.querySelector('#nextFilesButton').removeAttribute('disabled');
            }
        });
    }
}

fileModal.addEventListener('show.bs.modal', function (event) {
    let page = 0;
    let button = event.relatedTarget
    let deps_path = button.getAttribute('data-bs-pid')
    let maxResults=50
    let modalTitle = fileModal.querySelector('.modal-title')
    modalTitle.textContent = 'Dependencies of: ' + deps_path
    let opens_div = fileModal.querySelector('.file_contents')
    let filename = fileModal.querySelector('.module_filename')
    let original_path = fileModal.querySelector('.module_path')
    let ppid = fileModal.querySelector('.module_parent')
    let type = fileModal.querySelector('.module_type')
    let access = fileModal.querySelector('.module_access')
    let exists = fileModal.querySelector('.module_exists')
    let link = fileModal.querySelector('.module_link')
    let files_pages = fileModal.querySelector('.files_page_numbers')
    fileModal.querySelector('#prevFilesButton').setAttribute('disabled', '');
    fileModal.querySelector('#nextFilesButton').removeAttribute('disabled');
    let request = `${ctx}linked_modules?filter=[path=${deps_path}]&details=true`;
    $.get(request, function (data) {
        let entry = data.entries[0];
        filename.innerText = entry.filename;
        original_path.innerText = entry.original_path;
        ppid.innerHTML = `<a href="${ctx}proc_tree?pid=${entry.ppid}" target="_blank">${entry.ppid}</a>`;
        type.innerText = entry.type;
        access.innerText = entry.access;
        exists.innerText = entry.exists;
        link.innerText = entry.link;
    });
    request = `${ctx}deps_for?path=${deps_path}&page=${page}&entries-per-page=${maxResults}&cached=true`;
    $.get(request, function (data) {
        if (Math.ceil(data.count/maxResults) < 2) {
            fileModal.querySelector('#nextFilesButton').setAttribute('disabled', '');
        }
        files_pages.innerText = (page + 1).toString() + "/" + Math.ceil(data.count/maxResults).toString()
        opens_div.innerHTML = ""
        if (data["entries"].length > 0 ){
            if(fileModal.querySelector('#headingFiles').children[0].classList.contains("collapsed")){
                fileModal.querySelector('#headingFiles').children[0].click();
            }
            fileModal.querySelector('#openModal').style.display = "block";
        }
        for (d in data["entries"]) {
            opens_div.innerHTML += "<p class='file_mode_r'>" + data["entries"][d] + "</p>"
        }
    });
})
fileModal.addEventListener('hidden.bs.modal', function (event) {
    fileModal.querySelector('#openModal').style.display = "none";
    fileModal.querySelector('.file_contents').innerHTML = "";
})
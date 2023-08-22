let fileModal = document.getElementById('proc_info_modal')

function nextFiles() {
    let pid_div = fileModal.querySelector('.proc_id');
    let opens_div = fileModal.querySelector('.file_contents');
    let files_pages = fileModal.querySelector('.files_page_numbers');
    let page = parseInt(files_pages.innerText.split("/")[0]) - 1;
    let pid = pid_div.innerText.split(":")[0];
    let idx = pid_div.innerText.split(":")[1];
    if (page < (parseInt(files_pages.innerText.split("/")[1]) - 1)) {
        page++;
        let request = '/proc?pid=' + pid + "&idx=" + idx + "&page=" + page;
        $.get(request, function (data) {
            Array.from(opens_div.getElementsByTagName("p")).forEach(element => {
                element.remove();
            });
            for (let d in data["openfiles"]) {
                let mode_file = data["openfiles"][d];
                opens_div.innerHTML += "<p class='file_mode_" + mode_file[0] + "'>" + mode_file[1] + "</p>"
            }
            files_pages.innerText = (page + 1) + "/" + data["files_pages"]
            if (page > 0) {
                fileModal.querySelector('#prevFilesButton').removeAttribute('disabled');
            }
            if (page === parseInt(files_pages.innerText.split("/")[1]) - 1) {
                fileModal.querySelector('#nextFilesButton').setAttribute('disabled', '');
            }
        });
    }
}

function prevFiles() {
    let pid_div = fileModal.querySelector('.proc_id');
    let opens_div = fileModal.querySelector('.file_contents');
    let files_pages = fileModal.querySelector('.files_page_numbers');
    let page = parseInt(files_pages.innerText.split("/")[0]) - 1;
    let pid = pid_div.innerText.split(":")[0];
    let idx = pid_div.innerText.split(":")[1];
    if (page > 0) {
        page--;
        let request = '/proc?pid=' + pid + "&idx=" + idx + "&page=" + page;
        $.get(request, function (data) {
            Array.from(opens_div.getElementsByTagName("p")).forEach(element => {
                element.remove();
            });
            for (let d in data["openfiles"]) {
                let mode_file = data["openfiles"][d]
                opens_div.innerHTML += "<p class='file_mode_" + mode_file[0] + "'>" + mode_file[1] + "</p>"
            }
            files_pages.innerText = (page + 1) + "/" + data["files_pages"]
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
    let proc_id = button.getAttribute('data-bs-pid')
    let pid = proc_id.split(":")[0]
    let idx = proc_id.split(":")[1]

    let modalTitle = fileModal.querySelector('.modal-title')
    modalTitle.textContent = 'Opens of process pid: ' + pid + ' idx: ' + idx
    let opens_div = fileModal.querySelector('.file_contents')
    let pid_div = fileModal.querySelector('.proc_id')
    let duration_div = fileModal.querySelector('.proc_duration')
    let parent_div = fileModal.querySelector('.proc_parent')
    let proc_cwd = fileModal.querySelector('.proc_cwd')
    let proc_bin = fileModal.querySelector('.proc_bin')
    let proc_cmd = fileModal.querySelector('.proc_cmd')
    let child_count_div = fileModal.querySelector('.proc_child_count')
    let files_pages = fileModal.querySelector('.files_page_numbers')
    let proc_details_div = fileModal.querySelector('.process_details')
    fileModal.querySelector('#prevFilesButton').setAttribute('disabled', '');
    fileModal.querySelector('#nextFilesButton').removeAttribute('disabled');
    let request = '/proc?pid=' + pid + "&idx=" + idx + "&page=" + page
    $.get(request, function (data) {
        pid_div.innerText = data.pid + ":" + data.idx;
        duration_div.innerText = nanoToStr(data["etime"])
        parent_div.innerText = data["ppid"] + ":" + data["pidx"];
        child_count_div.innerText = data.children.length
        proc_cwd.innerText = data["cwd"]
        proc_bin.innerText = data["bin"]
        proc_cmd.innerText = data.cmd.join(" ")
        if (data.files_pages < 2) {
            fileModal.querySelector('#nextFilesButton').setAttribute('disabled', '');
        }
        files_pages.innerText = (page + 1).toString() + "/" + data["files_pages"].toString()
        opens_div.innerHTML = ""
        if (data["openfiles"].length > 0 ){
            fileModal.querySelector('#openModal').style.display = "block";
        }
        for (d in data["openfiles"]) {
            let mode_file = data["openfiles"][d]
            opens_div.innerHTML += "<p class='file_mode_" + mode_file[0] + "'>" + mode_file[1] + "</p>"
        }
        if (data.class === "compiler") {
            fileModal.querySelector('#compilerModal').style.display = "block";
            fileModal.querySelector('.src_type').innerText = data["src_type"];
            var compiled_div = fileModal.querySelector('.compiled_list');
            for (d in data.compiled) {
                var mode_file  = Object.entries(data.compiled[d])
                compiled_div.innerHTML += "<p><span class='badge bg-success me-1'>"+mode_file[0][1]+"</span> "+ mode_file[0][0] +"</p>";
            }

            fileModal.querySelector('.objects_list').innerHTML = data["objects"].map((val,idx) => { return `<p>${val}</p>`; }).join('\n')
            fileModal.querySelector('.headers_list').innerHTML = data["headers"].map((val,idx) => { return `<p>${val}</p>`; }).join('\n')
            fileModal.querySelector('.ipaths_list').innerHTML = data["ipaths"].map((val,idx) => { return `<p>${val}</p>`; }).join('\n')
            fileModal.querySelector('.defs_list').innerHTML = data["defs"].map((val,idx) => { return `<p><span class="col-4">${val[0]}</span>=<span class="col-4">${val[1]}</span></p>`; }).join('\n')
            fileModal.querySelector('.undefs_list').innerHTML = data["undefs"].map((val,idx) => { return `<p><span class="col-4">${val[0]}</span>=<span class="col-4">${val[1]}</span></p>`; }).join('\n')

        }
        if (data.class === "linker") {
            fileModal.querySelector('#linkerModal').style.display = "block";
            let linked_div = fileModal.querySelector('.linked_list');
            linked_div.innerHTML += "<p>" + data["linked"] + "</p>"
        }
    });
})
fileModal.addEventListener('hidden.bs.modal', function (event) {
    fileModal.querySelector('#openModal').style.display = "none";
    fileModal.querySelector('.file_contents').innerHTML = "";
    fileModal.querySelector('#compilerModal').style.display = "none";
    fileModal.querySelector('.src_type').innerText = "";
    fileModal.querySelector('.compiled_list').innerText = "";
    fileModal.querySelector('.objects_list').innerText = "";
    fileModal.querySelector('.headers_list').innerText = "";
    fileModal.querySelector('.ipaths_list').innerText = "";
    fileModal.querySelector('.defs_list').innerText = "";
    fileModal.querySelector('.undefs_list').innerText = "";
    fileModal.querySelector('#linkerModal').style.display = "none";
    fileModal.querySelector('.linked_list').innerText = "";
})
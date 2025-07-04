$(document).ready(function () {
    let b = $("body");

    b.unbind().on("click", ".caret", fillChild);

    b.on("click", ".caretDone", async function () {
        if(this.parentNode.parentNode.getElementsByClassName('navigation').length > 0){
            this.parentNode.parentNode.getElementsByClassName('navigation')[0].style.display = "none";
        }
        let parent = this.parentNode.parentNode;
        let childrenList = $(parent).children('.nested');
        $(childrenList[0]).css("display", "none");
        $(this).addClass('caretReady');
        $(this).removeClass('caretDone');
    });

    b.on("click", ".caretReady", async function () {
        if(this.parentNode.parentNode.getElementsByClassName('navigation').length > 0){
            this.parentNode.parentNode.getElementsByClassName('navigation')[0].style.display = "";
        }
        let parent = this.parentNode.parentNode;
        let childrenList = $(parent).children('.nested');
        $(childrenList[0]).css("display", "block");
        $(this).removeClass('caretReady');
        $(this).addClass('caretDone');
    });

});

async function fillChild() {
    let id = $(this)[0].id;
    let path = $(this)[0].getAttribute("data-path");
    get_childs(path, id, this);
}

function get_childs(path,id, node, page = 0, lm) {
    if(node.parentNode.parentNode.getElementsByClassName('navigation').length > 0){
        node.parentNode.parentNode.getElementsByClassName('navigation')[0].remove();
    }
    let deps_url = `${ctx}deps_of?path=${encodeURIComponent(path)}&page=${page}&direct=true`;
    const request = new XMLHttpRequest();
    request.open("GET", deps_url, false);
    request.send(null);

    if (request.status === 200) {
    }

    let data = JSON.parse(request.responseText);
    let maxPage = parseInt(data.page_max);
    let maxResults = 10;
    if (parseInt(maxPage) > 1) {
        let divPag = $('<div class="row navigation align-middle ml-10"/>');
        divPag.attr('id', 'nav'+id);
        let navPag = $('<nav class="ml-20 col-md-auto"/>');
        let ulPag = $('<ul class="pagination pagination-sm"/>');
        let liPag = $('<li class="page-item "/>');
        liPag.addClass(page > 0 ? "" : "disabled");
        let buttonPag = $('<button class="page-link"/>');
        buttonPag.text("« First");
        buttonPag.click(function () {
            let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
            let lis = ul.children;
            let toSave = [];
            for (let linum = 0; linum < lis.length; linum++) {
                if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                    toSave.push(lis[linum])
                }
            }
            ul.innerHTML = '';
            toSave.forEach(element => ul.append(element));
            get_childs(path, id, node, 0);
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);
        liPag = $('<li class="page-item "/>');
        liPag.addClass(page > 0 ? "" : "disabled");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text("‹ Previous");
        buttonPag.click(function () {
            if (page > 0) {
                let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                let lis = ul.children;
                let toSave = [];
                for (let linum = 0; linum < lis.length; linum++) {
                    if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                        toSave.push(lis[linum])
                    }
                }
                ul.innerHTML = '';
                toSave.forEach(element => ul.append(element));
                
                get_childs(path, id, node, page-1);
            }
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        if(page - 1 > 0){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page-2);
            buttonPag.click(function () {
                if (page > 1) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(path, id, node, page-2);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }
        if(page  > 0){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page-1);
            buttonPag.click(function () {
                if (page > 0) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(path, id, node, page-1);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }
        liPag = $('<li class="page-item "/>');
        liPag.addClass("active");
        liPag.attr("aria-current", "page");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text(page);
        buttonPag.click(function () {
            let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
            let lis = ul.children;
            let toSave = [];
            for (let linum = 0; linum < lis.length; linum++) {
                if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                    toSave.push(lis[linum])
                }
            }
            ul.innerHTML = '';
            toSave.forEach(element => ul.append(element));
            get_childs(path, id, node, page);
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        if(page < maxPage-1){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page+1);
            buttonPag.click(function () {
                if (page < (parseInt(maxPage) - 1)) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(path, id, node, page+1);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }

        if(page+1 < maxPage-1){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page+2);
            buttonPag.click(function () {
                if (page < (parseInt(maxPage) - 2)) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(path, id, node, page+2);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }
        liPag = $('<li class="page-item "/>');
        liPag.addClass(page < maxPage-1 ? "" : "disabled");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text("Next ›");
        buttonPag.click(function () {
            if (page < (parseInt(maxPage) - 1)) {
                let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                let lis = ul.children;
                let toSave = [];
                for (let linum = 0; linum < lis.length; linum++) {
                    if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                        toSave.push(lis[linum])
                    }
                }
                ul.innerHTML = '';
                toSave.forEach(element => ul.append(element));
                get_childs(path, id, node, page+1);
            }
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        liPag = $('<li class="page-item "/>');
        liPag.addClass(page < maxPage-1 ? "" : "disabled");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text("Last »");
        buttonPag.click(function () {
            if (page < (parseInt(maxPage) - 1)) {
                let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                let lis = ul.children;
                let toSave = [];
                for (let linum = 0; linum < lis.length; linum++) {
                    if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                        toSave.push(lis[linum])
                    }
                }
                ul.innerHTML = '';
                toSave.forEach(element => ul.append(element));
                get_childs(path, id, node, maxPage-1);
            }
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        navPag.append(ulPag);
        divPag.append(navPag);
        let spanPag = $('<span class = "col-md-auto" style="margin-top:10px"/>');
        spanPag.append($('<span style="font-weight:100"/>').text("Showing "));
        spanPag.append(page * maxResults + 1+'-'+Math.min((page+1) * maxResults, data["count"]));
        spanPag.append($('<span style="font-weight:100"/>').text(" of "));
        spanPag.append(data["count"]);
        divPag.append(spanPag);
        node.parentNode.parentNode.children[0].after(divPag[0]);
    }
    let process = $('<ul class="nested" />');
    if (node.parentNode.parentNode.getElementsByClassName("nested").length) {
        process = $(node.parentNode.parentNode.getElementsByClassName("nested")[0]);
    } else {
        process.appendTo(node.parentNode.parentNode);
    }
    let depArray = data["entries"];
    for (let i = 0; i < depArray.length; i++) {
        if (node.parentNode.parentNode.getElementsByClassName(depArray[i]["path"]+depArray[i]["parent"]).length > 0) {
            continue;
        }
        let li = $('<li/>');
        let div = $('<div/>');
        let caret = ($('<button />'));
        caret.attr('data-path', depArray[i]["path"]);
        if(depArray[i]["num_deps"]){
            caret.text("Linked Module");
        }
        else{
            caret.text("Linked Module");
        }
        caret.addClass(depArray[i]["path"]+depArray[i]["parent"]);
        caret.addClass('pid col-1 btn btn-sm me-1');
        if (depArray[i]['num_deps'] > 0) {
            caret.addClass('caret');
            caret.addClass('btn-primary');
            let child_len = $('<span class="badge rounded-pill bg-danger child_len"/>').text(depArray[i]["num_deps"]);
            caret.append(child_len);
        } else {
            caret.addClass('btn-outline-primary');
            caret.addClass('nonCaret');
            caret.attr('disabled', 'disabled');
        }
        div.append(caret);
        div.append($('<button type="button" class="btn btn-outline-primary btn-sm" data-bs-toggle="modal" data-bs-target="#proc_info_modal" data-len=' + depArray[i]["path"] + ' data-bs-pid="' + depArray[i]["path"]+ '">i</button>'));
        div.append($('<span />').text(depArray[i]["path"]));
        if (depArray[i]["num_deps"] > 0) {
            div.append($('<span />').append($(`<a href="${ctx}deps_tree?path=${depArray[i]["path"]}" target="_blank" title="Start from this process" style="text-decoration:none" />`).text(" ↸ ")));
        }
        li.append(div);
        process.append(li);
    }

    $(node).removeClass('caret');
    $(node).addClass('caretDone');
}

function render_pagination(container_id, cur_page, max_page, count, entries_per_page, origin_url){
    cur_page = parseInt(cur_page, 10);
    max_page = parseInt(max_page, 10);
    count = parseInt(count, 10);
    entries_per_page = parseInt(entries_per_page, 10);
    let pagination_nav = document.getElementById(container_id);

    pagination_nav.innerHTML = `<nav class="ml-20 col-md-auto"><ul class="pagination pagination-sm">

    <li class="page-item ${cur_page > 0 ? '' :' disabled' }">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page=0')}">« First</a>
    </li>
    <li class="page-item ${cur_page > 0 ? '' :' disabled' }">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page='+ (cur_page - 1))}">‹ Previous</a>
    </li>` +
    ((cur_page - 1) > 0 ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page='+ (cur_page - 2))}">${cur_page - 2}</a>
    </li>
    `: ``) +
    (cur_page > 0 ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page='+ (cur_page - 1))}">${cur_page - 1}</a>
    </li>
    `: ``) +
    `<li class="page-item active" aria-current="page">
        <a class="page-link" href="#">${cur_page}</a>
    </li>
    `+
    (cur_page < max_page ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page='+ (cur_page + 1))}">${cur_page + 1}</a>
    </li>
    `: ``) +
    ((cur_page + 1) < max_page ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page='+ (cur_page + 2))}">${cur_page + 2}</a>
    </li>
    `: ``) +
    `<li class="page-item` + (cur_page < max_page ? '' :' disabled') +`">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page='+ (cur_page + 1))}">Next ›</a>
    </li>
    <li class="page-item` + (cur_page < max_page ? '' :' disabled') +`">
        <a class="page-link" href="${ origin_url.replace('page='+ cur_page ,'page='+ (max_page))}">Last »</a>
    </li>
</ul></nav>
 <span class="col-md-auto"><span style="font-weight:100">Showing</span> ${cur_page * entries_per_page + 1}-${ Math.min((cur_page+1) * entries_per_page, count) } <span style="font-weight:100">of</span> ${count}</span>
`;
    pagination_nav.style.display = "";
}

function searchModule(btn){
    let search_str = btn.previousElementSibling.value;
    window.location.replace(`${ctx}deps_tree?filter=${search_str}`)
}
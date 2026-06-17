// ===== 导航指引功能 =====

function initNavigationPage() {
    // 楼层按钮点击切换地图
    document.querySelectorAll('.floor-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.querySelectorAll('.floor-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            drawLibraryMap(this.getAttribute('data-floor'));
        });
    });
    
    // 初始绘制一楼地图
    drawLibraryMap('1');
}

// 根据楼层绘制地图
function drawLibraryMap(floor) {
    const svg = document.getElementById('library-map-svg');
    if (!svg) return;
    svg.innerHTML = '';
    
    // 添加背景
    const bg = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    bg.setAttribute('width', '800');
    bg.setAttribute('height', '300');
    bg.setAttribute('fill', '#f5f5f5');
    svg.appendChild(bg);
    
    // 添加楼层标题
    const title = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    title.setAttribute('x', '400');
    title.setAttribute('y', '30');
    title.setAttribute('text-anchor', 'middle');
    title.setAttribute('font-size', '22');
    title.setAttribute('fill', '#333');
    title.setAttribute('font-weight', 'bold');
    title.textContent = `图书馆 ${floor}楼 平面图`;
    svg.appendChild(title);
    
    // 定义各楼层书架布局
    const floorLayouts = {
        '1': [
            { x: 80, y: 70, width: 140, height: 55, label: 'A区' },
            { x: 280, y: 70, width: 140, height: 55, label: 'B区' },
            { x: 480, y: 70, width: 140, height: 55, label: 'C区' },
            { x: 80, y: 170, width: 140, height: 55, label: 'D区' },
            { x: 280, y: 170, width: 140, height: 55, label: 'E区' },
            { x: 480, y: 170, width: 140, height: 55, label: 'F区' },
            { x: 680, y: 70, width: 90, height: 155, label: '服务台' }
        ],
        '2': [
            { x: 80, y: 70, width: 140, height: 55, label: 'G区' },
            { x: 280, y: 70, width: 140, height: 55, label: 'H区' },
            { x: 480, y: 70, width: 140, height: 55, label: 'I区' },
            { x: 80, y: 170, width: 140, height: 55, label: 'J区' },
            { x: 280, y: 170, width: 140, height: 55, label: 'K区' },
            { x: 480, y: 170, width: 140, height: 55, label: 'L区' },
            { x: 680, y: 70, width: 90, height: 155, label: '阅览区' }
        ],
        '3': [
            { x: 80, y: 70, width: 180, height: 55, label: 'M区' },
            { x: 320, y: 70, width: 180, height: 55, label: 'N区' },
            { x: 560, y: 70, width: 160, height: 55, label: 'O区' },
            { x: 80, y: 170, width: 180, height: 55, label: 'P区' },
            { x: 320, y: 170, width: 180, height: 55, label: 'Q区' },
            { x: 560, y: 170, width: 160, height: 55, label: 'R区' }
        ],
        '4': [
            { x: 120, y: 70, width: 200, height: 55, label: 'S区' },
            { x: 380, y: 70, width: 200, height: 55, label: 'T区' },
            { x: 120, y: 170, width: 200, height: 55, label: 'U区' },
            { x: 380, y: 170, width: 200, height: 55, label: 'V区' },
            { x: 640, y: 100, width: 130, height: 100, label: '自习室' }
        ]
    };
    
    const areas = floorLayouts[floor] || floorLayouts['1'];
    
    // 绘制书架区域
    areas.forEach(area => {
        const shelf = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        shelf.setAttribute('x', area.x.toString());
        shelf.setAttribute('y', area.y.toString());
        shelf.setAttribute('width', area.width.toString());
        shelf.setAttribute('height', area.height.toString());
        shelf.setAttribute('fill', '#FF9800');
        shelf.setAttribute('stroke', '#E65100');
        shelf.setAttribute('stroke-width', '2');
        shelf.setAttribute('rx', '8');
        svg.appendChild(shelf);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', (area.x + area.width / 2).toString());
        label.setAttribute('y', (area.y + area.height / 2).toString());
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('dominant-baseline', 'middle');
        label.setAttribute('font-size', '18');
        label.setAttribute('font-weight', 'bold');
        label.setAttribute('fill', '#FFF');
        label.textContent = area.label;
        svg.appendChild(label);
    });
}